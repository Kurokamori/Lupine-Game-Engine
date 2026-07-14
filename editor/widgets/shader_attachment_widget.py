"""
Shader Attachment Widget

Generic inspector widget for attaching a Lupine Shader (.lsh) to a 2D UI component
(ColorRect, Image2D, Panel, Shape2D) via its material-override slot, introspecting the
shader's exported parameters, and editing those parameter values per-instance.

The shader path is stored in the component's `materialOverride` property (res:// path) and
the edited parameter values are stored as a JSON object string in the component's
`shaderParameters` property. Both are consumed by the engine at render time.
"""

import os
import json
from pathlib import Path

from PyQt6.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QLabel, QPushButton,
                             QLineEdit, QFrame, QFileDialog, QMessageBox)
from PyQt6.QtCore import pyqtSignal
from PyQt6.QtGui import QFont

from editor.theme import get_theme_manager
from editor.widgets.shader_definitions import ParameterType, parse_lsh_file


class ShaderAttachmentWidget(QWidget):
    """Attach + edit a .lsh shader and its exported parameters for a 2D UI component."""

    value_changed = pyqtSignal(object)  # Emitted when the shader or a parameter changes

    def __init__(self, component, editor_bridge, main_editor=None,
                 default_template="Color Rect (2D UI)", parent=None):
        super().__init__(parent)
        self.component = component
        self.editor_bridge = editor_bridge
        self.main_editor = main_editor
        self.default_template = default_template

        self._shader_path = ""      # res:// path stored on the component
        self._param_values = {}     # uniform_name -> {"type": str, "value": ...}
        self._param_widgets = {}    # uniform_name -> property widget
        self._params = []           # list[ShaderParameter] from .lsh introspection

        self._init_ui()
        self._load_from_component()

    # ------------------------------------------------------------------ logging
    def _log(self, message, level="Debug"):
        if (self.main_editor and hasattr(self.main_editor, 'panels')
                and 'console' in self.main_editor.panels):
            self.main_editor.panels['console'].log_message(f"[ShaderAttachment] {message}", level)

    # ------------------------------------------------------------------ UI setup
    def _init_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 4, 0, 4)
        layout.setSpacing(4)

        theme = get_theme_manager().get_current_theme()
        text_color = theme.colors.text_primary if theme else "#ffffff"

        title = QLabel("Material Override (Shader)")
        title_font = QFont()
        title_font.setBold(True)
        title_font.setPointSize(10)
        title.setFont(title_font)
        title.setStyleSheet(f"color: {text_color}; padding: 2px;")
        layout.addWidget(title)

        # Attached shader path (read-only display)
        path_row = QHBoxLayout()
        path_row.setSpacing(4)
        path_label = QLabel("Shader:")
        path_label.setFixedWidth(48)
        path_row.addWidget(path_label)
        self.path_edit = QLineEdit()
        self.path_edit.setReadOnly(True)
        self.path_edit.setPlaceholderText("None (built-in rendering)")
        path_row.addWidget(self.path_edit, 1)
        layout.addLayout(path_row)

        # Action buttons
        btn_row = QHBoxLayout()
        btn_row.setSpacing(4)
        self.browse_btn = QPushButton("Browse")
        self.browse_btn.setToolTip("Attach an existing .lsh shader")
        self.browse_btn.clicked.connect(self._browse_shader)
        btn_row.addWidget(self.browse_btn)

        self.new_btn = QPushButton("New")
        self.new_btn.setToolTip("Create a new shader and attach it")
        self.new_btn.clicked.connect(self._new_shader)
        btn_row.addWidget(self.new_btn)

        self.edit_btn = QPushButton("Edit")
        self.edit_btn.setToolTip("Open the attached shader in the shader editor")
        self.edit_btn.clicked.connect(self._edit_shader)
        btn_row.addWidget(self.edit_btn)

        self.clear_btn = QPushButton("Clear")
        self.clear_btn.setToolTip("Detach the shader (use the built-in rendering)")
        self.clear_btn.clicked.connect(self._clear_shader)
        btn_row.addWidget(self.clear_btn)
        layout.addLayout(btn_row)

        # Exported-parameter widgets container
        self.params_frame = QFrame()
        self.params_layout = QVBoxLayout(self.params_frame)
        self.params_layout.setContentsMargins(0, 2, 0, 2)
        self.params_layout.setSpacing(2)
        layout.addWidget(self.params_frame)

    # ------------------------------------------------------ component property IO
    def _get_component_string(self, prop_name):
        try:
            raw = self.editor_bridge.get_component_property(self.component, prop_name)
            if not raw:
                return ""
            val = json.loads(raw) if isinstance(raw, str) else raw
            return val if isinstance(val, str) else ""
        except Exception as e:
            self._log(f"get '{prop_name}' failed: {e}", "Warning")
            return ""

    def _set_component_string(self, prop_name, value):
        try:
            self.editor_bridge.set_component_property(self.component, prop_name, json.dumps(value))
        except Exception as e:
            self._log(f"set '{prop_name}' failed: {e}", "Warning")

    def _load_from_component(self):
        # The shader path lives in the component's material-override slot.
        self._shader_path = self._get_component_string("materialOverride")

        self._param_values = {}
        params_json = self._get_component_string("shaderParameters")
        if params_json:
            try:
                parsed = json.loads(params_json)
                if isinstance(parsed, dict):
                    self._param_values = parsed
            except json.JSONDecodeError:
                self._param_values = {}

        self.path_edit.setText(self._shader_path)
        self._rebuild_parameters()

    # ------------------------------------------------------------- path helpers
    def _project_root(self):
        """Project root (res:// base), resolved via the engine's AssetDatabase singleton.

        Using the C++ singleton avoids the Python module-path duplication issue where the
        inspector module is imported under two names (panels.inspector_panel vs
        editor.panels.inspector_panel), each with its own (possibly unset) _project_root.
        """
        try:
            import lupine_engine as le
            db = le.AssetDatabase.get_instance()
            if db.is_initialized():
                root = db.get_project_root()
                if root:
                    return root
        except Exception:
            pass
        # Fall back to the inspector module global, trying both import paths.
        for mod in ("panels.inspector_panel", "editor.panels.inspector_panel"):
            try:
                module = __import__(mod, fromlist=["get_project_root"])
                root = module.get_project_root()
                if root:
                    return root
            except Exception:
                continue
        return ""

    def _resolve_abs_path(self, res_path):
        if not res_path:
            return ""

        # Preferred: the engine's asset resolver (handles res:// and UUIDs).
        try:
            import lupine_engine as le
            asset_db = le.AssetDatabase.get_instance()
            if asset_db.is_initialized():
                resolved = asset_db.resolve_asset(res_path)
                if resolved and os.path.exists(resolved):
                    return os.path.normpath(resolved)
        except Exception:
            pass

        # Fallback: join the res:// path against the project root.
        if res_path.startswith("res://"):
            root = self._project_root()
            if root:
                candidate = os.path.normpath(os.path.join(root, res_path[6:]))
                if os.path.exists(candidate):
                    return candidate
            return ""

        # Already an absolute/physical path.
        return res_path if os.path.exists(res_path) else ""

    # ----------------------------------------------------------------- actions
    def _browse_shader(self):
        from editor.panels.inspector_panel import convert_to_res_path
        start_dir = self._project_root()
        file_path, _ = QFileDialog.getOpenFileName(
            self, "Select Lupine Shader", start_dir,
            "Lupine Shader Files (*.lsh);;All Files (*)")
        if file_path:
            self._attach_shader(convert_to_res_path(file_path))

    def _new_shader(self):
        from editor.dialogs.new_shader_dialog import NewShaderDialog
        from editor.panels.inspector_panel import convert_to_res_path

        root = self._project_root()
        dialog = NewShaderDialog(self, default_location=root)

        # Default to the component-appropriate template.
        idx = dialog.template_combo.findText(self.default_template)
        if idx >= 0:
            dialog.template_combo.setCurrentIndex(idx)

        if dialog.exec() != NewShaderDialog.DialogCode.Accepted:
            return

        save_path = dialog.get_save_path()
        source = dialog.get_shader_source()
        try:
            Path(save_path).parent.mkdir(parents=True, exist_ok=True)
            with open(save_path, 'w', encoding='utf-8') as f:
                f.write(source)
        except OSError as e:
            QMessageBox.warning(self, "Error", f"Failed to create shader:\n{e}")
            return

        # Open the new shader in the shader editor, then attach it.
        if self.main_editor and hasattr(self.main_editor, '_open_shader_in_editor'):
            self.main_editor._open_shader_in_editor(save_path)
        self._attach_shader(convert_to_res_path(save_path))

    def _edit_shader(self):
        abs_path = self._resolve_abs_path(self._shader_path)
        if not abs_path or not os.path.exists(abs_path):
            QMessageBox.information(self, "No Shader", "Attach a .lsh shader first.")
            return
        if self.main_editor and hasattr(self.main_editor, '_open_shader_in_editor'):
            self.main_editor._open_shader_in_editor(abs_path)

    def _clear_shader(self):
        self._attach_shader("")

    def _attach_shader(self, res_path):
        self._shader_path = res_path
        self.path_edit.setText(res_path)
        self._set_component_string("materialOverride", res_path)

        # Reset parameter values when the shader changes, then seed defaults.
        self._param_values = {}
        self._rebuild_parameters()
        self._write_all_defaults()

        self._mark_dirty()
        self.value_changed.emit(res_path)

    def _mark_dirty(self):
        if self.editor_bridge and hasattr(self.editor_bridge, 'mark_scene_dirty'):
            try:
                self.editor_bridge.mark_scene_dirty()
            except Exception:
                pass

    # ------------------------------------------------------- save-time refresh
    def refresh_shader(self, saved_abs_path):
        """Re-introspect parameters when the attached .lsh is saved in the editor."""
        attached_abs = self._resolve_abs_path(self._shader_path)
        if not attached_abs:
            return
        if os.path.normpath(saved_abs_path) == os.path.normpath(attached_abs):
            self._rebuild_parameters()

    # --------------------------------------------------------------- parameters
    def _rebuild_parameters(self):
        # Remove existing parameter widgets.
        while self.params_layout.count():
            item = self.params_layout.takeAt(0)
            if item.widget():
                item.widget().deleteLater()
        self._param_widgets = {}
        self._params = []

        # Only .lsh material overrides expose editable parameters.
        if not self._shader_path.lower().endswith(".lsh"):
            return

        abs_path = self._resolve_abs_path(self._shader_path)
        if not abs_path or not os.path.exists(abs_path):
            label = QLabel("Shader file not found")
            label.setStyleSheet("color: #c08080; font-style: italic;")
            self.params_layout.addWidget(label)
            return

        self._params = parse_lsh_file(abs_path)
        if not self._params:
            label = QLabel("No exported parameters")
            label.setStyleSheet("color: #888; font-style: italic;")
            self.params_layout.addWidget(label)
            return

        for param in self._params:
            widget = self._create_param_widget(param)
            if widget:
                self.params_layout.addWidget(widget)
                self._param_widgets[param.name] = widget

    def _stored_value(self, name):
        stored = self._param_values.get(name)
        if isinstance(stored, dict):
            return stored.get("value")
        return None

    def _create_param_widget(self, param):
        from editor.panels.inspector_panel import (
            FloatPropertyWidget, ColorPropertyWidget, PathPropertyWidget,
            Vec2PropertyWidget, Vec3PropertyWidget, Vec4PropertyWidget,
            IntPropertyWidget, BoolPropertyWidget)

        stored = self._stored_value(param.name)

        if param.param_type == ParameterType.FLOAT:
            widget = FloatPropertyWidget(param.display_name)
            value = stored if stored is not None else (param.default_value or 0.0)
            widget.set_value(float(value))
            widget.value_changed.connect(
                lambda v, n=param.name: self._on_param_changed(n, "float", float(v)))
            return widget

        if param.param_type == ParameterType.INT:
            widget = IntPropertyWidget(param.display_name)
            value = stored if stored is not None else (param.default_value or 0)
            widget.set_value(int(value))
            widget.value_changed.connect(
                lambda v, n=param.name: self._on_param_changed(n, "int", int(v)))
            return widget

        if param.param_type == ParameterType.BOOL:
            widget = BoolPropertyWidget(param.display_name)
            value = stored if stored is not None else bool(param.default_value)
            widget.set_value(bool(value))
            widget.value_changed.connect(
                lambda v, n=param.name: self._on_param_changed(n, "bool", bool(v)))
            return widget

        if param.param_type == ParameterType.COLOR:
            widget = ColorPropertyWidget(param.display_name)
            rgba = stored if stored is not None else (param.default_value or (1.0, 1.0, 1.0, 1.0))
            # ColorPropertyWidget takes/emits an (r,g,b,a) tuple of 0-1 floats (not a QColor).
            widget.set_value([float(c) for c in list(rgba)[:4]])
            widget.value_changed.connect(
                lambda c, n=param.name: self._on_param_changed(n, "color", [float(x) for x in c]))
            return widget

        if param.param_type == ParameterType.VEC2:
            widget = Vec2PropertyWidget(param.display_name)
            value = stored if stored is not None else (param.default_value or (0.0, 0.0))
            widget.set_value(value)
            widget.value_changed.connect(
                lambda v, n=param.name: self._on_param_changed(n, "vec2", [float(x) for x in v]))
            return widget

        if param.param_type == ParameterType.VEC3:
            widget = Vec3PropertyWidget(param.display_name)
            value = stored if stored is not None else (param.default_value or (0.0, 0.0, 0.0))
            widget.set_value(value)
            widget.value_changed.connect(
                lambda v, n=param.name: self._on_param_changed(n, "vec3", [float(x) for x in v]))
            return widget

        if param.param_type == ParameterType.VEC4:
            widget = Vec4PropertyWidget(param.display_name)
            value = stored if stored is not None else (param.default_value or (0.0, 0.0, 0.0, 0.0))
            widget.set_value(value)
            widget.value_changed.connect(
                lambda v, n=param.name: self._on_param_changed(n, "vec4", [float(x) for x in v]))
            return widget

        if param.param_type == ParameterType.TEXTURE:
            widget = PathPropertyWidget(param.display_name)
            if stored:
                try:
                    widget.set_value(stored)
                except Exception:
                    pass
            widget.value_changed.connect(
                lambda v, n=param.name: self._on_param_changed(n, "texture", v))
            return widget

        return None

    def _on_param_changed(self, uniform_name, type_name, value):
        self._param_values[uniform_name] = {"type": type_name, "value": value}
        self._write_params()
        self._mark_dirty()
        self.value_changed.emit(uniform_name)

    def _default_for(self, param):
        """Return (type_name, value) for a parameter's default, or (None, None)."""
        default = param.default_value
        if param.param_type == ParameterType.FLOAT:
            return "float", float(default) if default is not None else 0.0
        if param.param_type == ParameterType.INT:
            return "int", int(default) if default is not None else 0
        if param.param_type == ParameterType.BOOL:
            return "bool", bool(default)
        if param.param_type == ParameterType.COLOR:
            return "color", [float(c) for c in default] if default else [1.0, 1.0, 1.0, 1.0]
        if param.param_type == ParameterType.VEC2:
            return "vec2", [float(c) for c in default] if default else [0.0, 0.0]
        if param.param_type == ParameterType.VEC3:
            return "vec3", [float(c) for c in default] if default else [0.0, 0.0, 0.0]
        if param.param_type == ParameterType.VEC4:
            return "vec4", [float(c) for c in default] if default else [0.0, 0.0, 0.0, 0.0]
        return None, None

    def _write_all_defaults(self):
        """Seed any unset parameter with its default so the engine has values immediately."""
        for param in self._params:
            if param.name in self._param_values:
                continue
            type_name, value = self._default_for(param)
            if type_name is not None:
                self._param_values[param.name] = {"type": type_name, "value": value}
        self._write_params()

    def _write_params(self):
        # shaderParameters is a String property holding a JSON object string.
        self._set_component_string("shaderParameters", json.dumps(self._param_values))
