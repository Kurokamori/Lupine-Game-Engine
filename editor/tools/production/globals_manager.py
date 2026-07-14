"""
Globals Manager Tool
Manages global autoload scripts (singletons) and global variables for the Lupine Engine project
"""

from PyQt6.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QLabel,
                             QPushButton, QTabWidget, QTableWidget, QTableWidgetItem,
                             QFileDialog, QMessageBox, QHeaderView, QTextEdit,
                             QDialog, QFormLayout, QLineEdit, QComboBox, QDialogButtonBox,
                             QAbstractItemView, QInputDialog, QSizePolicy)
from PyQt6.QtCore import Qt, pyqtSignal
from PyQt6.QtGui import QFont
from pathlib import Path
import json
import sys
import os

# Import base panel
editor_path = Path(__file__).parent.parent.parent
if str(editor_path) not in sys.path:
    sys.path.insert(0, str(editor_path))

from panels.base_panel import EditorPanel
from theme import get_theme_manager


# Canonical global-variable type vocabulary. These mirror the engine's
# PropertyValueType names and are delivered to scripts through SetGlobalJson:
# scalars arrive as native scalars, structured types as native tables/dicts.
SUPPORTED_TYPES = [
    "int", "float", "double", "bool", "string", "resource",
    "vec2", "vec3", "vec4", "color", "quat", "rect",
    "array", "dictionary", "string_array", "int_array", "float_array",
]

# Structured object types encoded as JSON objects with a fixed field set, matching
# the engine's conventions (vec2 {x,y}, color {r,g,b,a}, quat {w,x,y,z}, rect {x,y,w,h}).
_OBJECT_FIELDS = {
    "vec2": ["x", "y"],
    "vec3": ["x", "y", "z"],
    "vec4": ["x", "y", "z", "w"],
    "color": ["r", "g", "b", "a"],
    "quat": ["w", "x", "y", "z"],
    "rect": ["x", "y", "w", "h"],
}

_TYPE_PLACEHOLDERS = {
    "int": "e.g., 100",
    "float": "e.g., 3.14",
    "double": "e.g., 3.141592653589793",
    "bool": "true or false",
    "string": "e.g., Hello World",
    "resource": "e.g., res://data/config.ares",
    "vec2": '{"x": 0, "y": 0}',
    "vec3": '{"x": 0, "y": 0, "z": 0}',
    "vec4": '{"x": 0, "y": 0, "z": 0, "w": 0}',
    "color": '{"r": 1, "g": 1, "b": 1, "a": 1}',
    "quat": '{"w": 1, "x": 0, "y": 0, "z": 0}',
    "rect": '{"x": 0, "y": 0, "w": 0, "h": 0}',
    "array": '[1, "two", true]',
    "dictionary": '{"key": "value"}',
    "string_array": '["a", "b", "c"]',
    "int_array": "[1, 2, 3]",
    "float_array": "[1.0, 2.5, 3.0]",
}


def default_global_value(var_type):
    """Return a sensible default value for a freshly created variable of var_type."""
    if var_type in ("int",):
        return 0
    if var_type in ("float", "double"):
        return 0.0
    if var_type == "bool":
        return False
    if var_type in ("string", "resource"):
        return ""
    if var_type in _OBJECT_FIELDS:
        return {field: (1.0 if var_type == "color" else 0.0) for field in _OBJECT_FIELDS[var_type]}
    if var_type == "quat":
        return {"w": 1.0, "x": 0.0, "y": 0.0, "z": 0.0}
    if var_type == "dictionary":
        return {}
    # array / string_array / int_array / float_array
    return []


def parse_global_value(var_type, value_str):
    """Parse the editor text for var_type into a JSON-serializable Python value.

    Returns (value, None) on success or (None, error_message) on failure.
    """
    value_str = value_str.strip()
    try:
        if var_type == "int":
            return int(value_str), None
        if var_type in ("float", "double"):
            return float(value_str), None
        if var_type == "bool":
            low = value_str.lower()
            if low not in ("true", "false"):
                return None, "Bool must be 'true' or 'false'"
            return low == "true", None
        if var_type in ("string", "resource"):
            return value_str, None

        # Structured types are entered as JSON.
        parsed = json.loads(value_str)

        if var_type in _OBJECT_FIELDS:
            if not isinstance(parsed, dict):
                return None, f"{var_type} must be a JSON object, e.g. {_TYPE_PLACEHOLDERS[var_type]}"
            result = {}
            for field in _OBJECT_FIELDS[var_type]:
                if field not in parsed:
                    return None, f"{var_type} is missing field '{field}'"
                component = parsed[field]
                if not isinstance(component, (int, float)) or isinstance(component, bool):
                    return None, f"{var_type} field '{field}' must be a number"
                result[field] = float(component)
            return result, None

        if var_type == "dictionary":
            if not isinstance(parsed, dict):
                return None, "dictionary must be a JSON object"
            return parsed, None

        if var_type == "array":
            if not isinstance(parsed, list):
                return None, "array must be a JSON array"
            return parsed, None

        if var_type in ("string_array", "int_array", "float_array"):
            if not isinstance(parsed, list):
                return None, f"{var_type} must be a JSON array"
            result = []
            for item in parsed:
                if var_type == "string_array":
                    if not isinstance(item, str):
                        return None, "string_array items must be strings"
                    result.append(item)
                elif var_type == "int_array":
                    if not isinstance(item, int) or isinstance(item, bool):
                        return None, "int_array items must be integers"
                    result.append(int(item))
                else:  # float_array
                    if not isinstance(item, (int, float)) or isinstance(item, bool):
                        return None, "float_array items must be numbers"
                    result.append(float(item))
            return result, None

        return None, f"Unsupported type '{var_type}'"
    except (ValueError, json.JSONDecodeError) as e:
        return None, str(e)


def format_global_value(var_type, value):
    """Return a single-line, editable string representation of a stored value."""
    if isinstance(value, bool):
        return "true" if value else "false"
    if isinstance(value, (dict, list)):
        return json.dumps(value)
    return str(value)


class AddSingletonDialog(QDialog):
    """Dialog for adding a new singleton/autoload script"""

    def __init__(self, project_dir, parent=None):
        super().__init__(parent)
        self.project_dir = project_dir
        self.script_path = ""
        self.global_name = ""

        self.setWindowTitle("Add Singleton")
        self.setMinimumSize(500, 200)  # Set both width and height minimums

        # Apply theme
        theme = get_theme_manager().get_current_theme()
        if theme:
            self.setStyleSheet(theme.get_stylesheet())

        self._setup_ui()

    def _setup_ui(self):
        """Setup dialog UI"""
        layout = QVBoxLayout(self)

        # Form layout
        form_layout = QFormLayout()

        # Script path selection
        script_layout = QHBoxLayout()
        self.script_path_edit = QLineEdit()
        self.script_path_edit.setReadOnly(True)
        self.script_path_edit.setPlaceholderText("Select a script file...")

        browse_btn = QPushButton("Browse...")
        browse_btn.clicked.connect(self._browse_script)

        script_layout.addWidget(self.script_path_edit)
        script_layout.addWidget(browse_btn)

        form_layout.addRow("Script Path:", script_layout)

        # Global class name
        self.global_name_edit = QLineEdit()
        self.global_name_edit.setPlaceholderText("e.g., GameManager, AudioController")
        form_layout.addRow("Global Name:", self.global_name_edit)

        layout.addLayout(form_layout)

        # Info label
        info_label = QLabel(
            "The script will be automatically loaded at runtime and accessible globally.\n"
            "Example: If global name is 'GameManager', access it via 'GameManager' in your scripts."
        )
        info_label.setProperty("secondary", True)
        info_label.setWordWrap(True)
        layout.addWidget(info_label)

        # Buttons
        button_box = QDialogButtonBox(
            QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel
        )
        button_box.accepted.connect(self._accept)
        button_box.rejected.connect(self.reject)
        layout.addWidget(button_box)

    def _browse_script(self):
        """Browse for script file"""
        file_dialog = QFileDialog(self)
        file_dialog.setWindowTitle("Select Script File")
        file_dialog.setDirectory(self.project_dir)
        file_dialog.setNameFilter("Scripts (*.py *.lua *.rb);;All Files (*.*)")

        if file_dialog.exec():
            selected_files = file_dialog.selectedFiles()
            if selected_files:
                # Convert to relative path
                abs_path = Path(selected_files[0])
                try:
                    rel_path = abs_path.relative_to(Path(self.project_dir))
                    self.script_path_edit.setText(str(rel_path).replace('/', '\\'))
                except ValueError:
                    # Path is not relative to project dir
                    QMessageBox.warning(
                        self,
                        "Invalid Path",
                        "Please select a script file within the project directory."
                    )

    def _accept(self):
        """Validate and accept"""
        self.script_path = self.script_path_edit.text().strip()
        self.global_name = self.global_name_edit.text().strip()

        if not self.script_path:
            QMessageBox.warning(self, "Validation Error", "Please select a script file.")
            return

        if not self.global_name:
            QMessageBox.warning(self, "Validation Error", "Please enter a global name.")
            return

        # Validate global name (must be valid identifier)
        if not self.global_name.isidentifier():
            QMessageBox.warning(
                self,
                "Validation Error",
                "Global name must be a valid identifier (letters, numbers, underscore, cannot start with number)."
            )
            return

        self.accept()

    def get_data(self):
        """Get the entered data"""
        return {
            "script_path": self.script_path,
            "global_name": self.global_name
        }


class AddVariableDialog(QDialog):
    """Dialog for adding/editing a global variable"""

    SUPPORTED_TYPES = SUPPORTED_TYPES

    def __init__(self, parent=None, edit_data=None):
        super().__init__(parent)
        self.edit_data = edit_data
        self.is_edit_mode = edit_data is not None

        self.setWindowTitle("Edit Variable" if self.is_edit_mode else "Add Variable")
        self.setMinimumSize(400, 180)  # Set both width and height minimums

        # Apply theme
        theme = get_theme_manager().get_current_theme()
        if theme:
            self.setStyleSheet(theme.get_stylesheet())

        self._setup_ui()

        # Load edit data if provided
        if self.is_edit_mode:
            self._load_edit_data()

    def _setup_ui(self):
        """Setup dialog UI"""
        layout = QVBoxLayout(self)

        # Form layout
        form_layout = QFormLayout()

        # Variable name
        self.name_edit = QLineEdit()
        self.name_edit.setPlaceholderText("e.g., player_health, max_score")
        form_layout.addRow("Name:", self.name_edit)

        # Variable type
        self.type_combo = QComboBox()
        self.type_combo.addItems(self.SUPPORTED_TYPES)
        self.type_combo.currentTextChanged.connect(self._on_type_changed)
        form_layout.addRow("Type:", self.type_combo)

        # Default value
        self.value_edit = QLineEdit()
        self.value_edit.setPlaceholderText("Default value")
        form_layout.addRow("Default Value:", self.value_edit)

        layout.addLayout(form_layout)

        # Buttons
        button_box = QDialogButtonBox(
            QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel
        )
        button_box.accepted.connect(self._accept)
        button_box.rejected.connect(self.reject)
        layout.addWidget(button_box)

    def _load_edit_data(self):
        """Load data for editing"""
        self.name_edit.setText(self.edit_data["name"])

        type_index = self.type_combo.findText(self.edit_data["type"])
        if type_index >= 0:
            self.type_combo.setCurrentIndex(type_index)

        self.value_edit.setText(format_global_value(self.edit_data["type"], self.edit_data["value"]))

    def _on_type_changed(self, type_name):
        """Update placeholder when type changes"""
        self.value_edit.setPlaceholderText(_TYPE_PLACEHOLDERS.get(type_name, ""))

    def _accept(self):
        """Validate and accept"""
        name = self.name_edit.text().strip()
        var_type = self.type_combo.currentText()
        value_str = self.value_edit.text().strip()

        if not name:
            QMessageBox.warning(self, "Validation Error", "Please enter a variable name.")
            return

        if not name.isidentifier():
            QMessageBox.warning(
                self,
                "Validation Error",
                "Variable name must be a valid identifier."
            )
            return

        if not value_str:
            QMessageBox.warning(self, "Validation Error", "Please enter a default value.")
            return

        # Validate value based on type
        _value, error = parse_global_value(var_type, value_str)
        if error is not None:
            QMessageBox.warning(
                self,
                "Validation Error",
                f"Invalid value for type {var_type}: {error}"
            )
            return

        self.accept()

    def get_data(self):
        """Get the entered data"""
        value_str = self.value_edit.text().strip()
        var_type = self.type_combo.currentText()

        value, _error = parse_global_value(var_type, value_str)

        return {
            "name": self.name_edit.text().strip(),
            "type": var_type,
            "value": value
        }


class GlobalsManagerTool(EditorPanel):
    """Main Globals Manager tool"""

    # Signal emitted when globals are modified
    globals_modified = pyqtSignal()

    def __init__(self, parent=None, project_dir=""):
        super().__init__("Globals Manager", parent)
        self.project_dir = project_dir
        self.globals_file = os.path.join(project_dir, "globals.json")

        # Data
        self.singletons = []  # List of {global_name: str, script_path: str}
        self.variables = []   # List of {name: str, type: str, value: any}

        # UI state
        self.variables_text_mode = False

        self._setup_ui()
        self._load_globals()

    def _setup_ui(self):
        """Setup tool UI"""
        # Set minimum size for the panel to ensure buttons are always visible
        self.setMinimumSize(400, 300)

        # Create tab widget
        self.tab_widget = QTabWidget()
        self.tab_widget.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Expanding)

        # Create tabs
        self.singletons_tab = self._create_singletons_tab()
        self.variables_tab = self._create_variables_tab()

        self.tab_widget.addTab(self.singletons_tab, "Singletons")
        self.tab_widget.addTab(self.variables_tab, "Variables")

        # Add to main layout (size policy will make it expand to fill available space)
        self.layout().addWidget(self.tab_widget)

    def _create_singletons_tab(self):
        """Create the singletons tab"""
        tab = QWidget()
        layout = QVBoxLayout(tab)
        layout.setContentsMargins(8, 8, 8, 8)
        layout.setSpacing(8)

        # Info label - fixed size, no stretch
        info_label = QLabel(
            "Autoload scripts (singletons) are loaded at runtime and accessible globally.\n"
            "Use them for manager classes, game controllers, and other persistent systems."
        )
        info_label.setProperty("secondary", True)
        info_label.setWordWrap(True)
        info_label.setMaximumHeight(50)
        layout.addWidget(info_label)

        # Buttons - fixed size at top, always visible
        btn_layout = QHBoxLayout()
        btn_layout.setSpacing(6)

        add_btn = QPushButton("Add Singleton")
        add_btn.clicked.connect(self._add_singleton)
        btn_layout.addWidget(add_btn)

        remove_btn = QPushButton("Remove Singleton")
        remove_btn.clicked.connect(self._remove_singleton)
        btn_layout.addWidget(remove_btn)

        open_script_btn = QPushButton("Open Script")
        open_script_btn.clicked.connect(self._open_singleton_script)
        btn_layout.addWidget(open_script_btn)

        btn_layout.addStretch()
        layout.addLayout(btn_layout)

        # Table - expandable with minimum size
        self.singletons_table = QTableWidget()
        self.singletons_table.setColumnCount(2)
        self.singletons_table.setHorizontalHeaderLabels(["Global Name", "Script Path"])

        # Set column resize modes for proper alignment
        # Global Name column: ResizeToContents for compact display
        self.singletons_table.horizontalHeader().setSectionResizeMode(0, QHeaderView.ResizeMode.ResizeToContents)
        # Script Path column: Stretch to fill remaining space
        self.singletons_table.horizontalHeader().setSectionResizeMode(1, QHeaderView.ResizeMode.Stretch)

        self.singletons_table.setSelectionBehavior(QAbstractItemView.SelectionBehavior.SelectRows)
        self.singletons_table.setSelectionMode(QAbstractItemView.SelectionMode.SingleSelection)
        self.singletons_table.verticalHeader().setVisible(False)
        self.singletons_table.setMinimumHeight(100)
        self.singletons_table.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Expanding)
        layout.addWidget(self.singletons_table)

        return tab

    def _create_variables_tab(self):
        """Create the variables tab"""
        tab = QWidget()
        layout = QVBoxLayout(tab)
        layout.setContentsMargins(8, 8, 8, 8)
        layout.setSpacing(8)

        # Info label - fixed size, no stretch
        info_label = QLabel(
            "Global variables are initialized at runtime and accessible from all scripts.\n"
            "Switch between table and text modes for different editing workflows."
        )
        info_label.setProperty("secondary", True)
        info_label.setWordWrap(True)
        info_label.setMaximumHeight(50)
        layout.addWidget(info_label)

        # Mode toggle - fixed size, no stretch
        mode_layout = QHBoxLayout()
        mode_layout.setSpacing(6)
        self.mode_toggle_btn = QPushButton("Switch to Text Mode")
        self.mode_toggle_btn.clicked.connect(self._toggle_variables_mode)
        mode_layout.addWidget(self.mode_toggle_btn)
        mode_layout.addStretch()
        layout.addLayout(mode_layout)

        # Buttons - fixed size at top, always visible
        btn_layout = QHBoxLayout()
        btn_layout.setSpacing(6)

        self.add_var_btn = QPushButton("Add Variable")
        self.add_var_btn.clicked.connect(self._add_variable)
        btn_layout.addWidget(self.add_var_btn)

        self.edit_var_btn = QPushButton("Edit Variable")
        self.edit_var_btn.clicked.connect(self._edit_variable)
        btn_layout.addWidget(self.edit_var_btn)

        self.remove_var_btn = QPushButton("Remove Variable")
        self.remove_var_btn.clicked.connect(self._remove_variable)
        btn_layout.addWidget(self.remove_var_btn)

        self.apply_text_btn = QPushButton("Apply Text Changes")
        self.apply_text_btn.setVisible(False)
        self.apply_text_btn.clicked.connect(self._apply_text_changes)
        btn_layout.addWidget(self.apply_text_btn)

        btn_layout.addStretch()
        layout.addLayout(btn_layout)

        # Container for table/text view - expandable
        # We'll add both views here with the same stretch, and toggle visibility
        # Table view
        self.variables_table = QTableWidget()
        self.variables_table.setColumnCount(3)
        self.variables_table.setHorizontalHeaderLabels(["Name", "Type", "Value"])

        # Set column resize modes for proper alignment
        # Name column: Interactive - allows user to resize, starts at content size
        self.variables_table.horizontalHeader().setSectionResizeMode(0, QHeaderView.ResizeMode.Interactive)
        # Type column: ResizeToContents - compact, fits "int", "float", "bool", "string"
        self.variables_table.horizontalHeader().setSectionResizeMode(1, QHeaderView.ResizeMode.ResizeToContents)
        # Value column: Stretch - fills remaining space
        self.variables_table.horizontalHeader().setSectionResizeMode(2, QHeaderView.ResizeMode.Stretch)

        # Set initial width for Name column
        self.variables_table.setColumnWidth(0, 200)

        self.variables_table.setSelectionBehavior(QAbstractItemView.SelectionBehavior.SelectRows)
        self.variables_table.setSelectionMode(QAbstractItemView.SelectionMode.SingleSelection)
        self.variables_table.verticalHeader().setVisible(False)
        self.variables_table.setSortingEnabled(True)
        self.variables_table.setMinimumHeight(100)
        self.variables_table.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Expanding)
        layout.addWidget(self.variables_table)

        # Text view
        self.variables_text = QTextEdit()
        self.variables_text.setVisible(False)
        self.variables_text.setPlaceholderText(
            "Enter variables, one per line:\n"
            "Format: VariableName Type DefaultValue\n\n"
            "Examples:\n"
            "player_health int 100\n"
            "gravity float 9.81\n"
            "debug_mode bool false\n"
            "player_name string Player1\n"
            "spawn_point vec2 {\"x\": 0, \"y\": 0}\n"
            "tint color {\"r\": 1, \"g\": 1, \"b\": 1, \"a\": 1}\n"
            "high_scores int_array [100, 80, 50]\n"
            "config dictionary {\"difficulty\": \"hard\"}"
        )
        font = QFont("Consolas", 10)
        self.variables_text.setFont(font)
        self.variables_text.setMinimumHeight(100)
        self.variables_text.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Expanding)
        layout.addWidget(self.variables_text)

        return tab

    def _toggle_variables_mode(self):
        """Toggle between table and text mode"""
        self.variables_text_mode = not self.variables_text_mode

        if self.variables_text_mode:
            # Switch to text mode
            self.mode_toggle_btn.setText("Switch to Table Mode")
            self.variables_table.setVisible(False)
            self.variables_text.setVisible(True)

            # Hide table buttons, show text buttons
            self.add_var_btn.setVisible(False)
            self.edit_var_btn.setVisible(False)
            self.remove_var_btn.setVisible(False)
            self.apply_text_btn.setVisible(True)

            # Convert table data to text
            self._variables_to_text()
        else:
            # Switch to table mode
            self.mode_toggle_btn.setText("Switch to Text Mode")
            self.variables_table.setVisible(True)
            self.variables_text.setVisible(False)

            # Show table buttons, hide text buttons
            self.add_var_btn.setVisible(True)
            self.edit_var_btn.setVisible(True)
            self.remove_var_btn.setVisible(True)
            self.apply_text_btn.setVisible(False)

    def _variables_to_text(self):
        """Convert variables list to text format"""
        lines = []
        for var in self.variables:
            value_str = format_global_value(var["type"], var["value"])
            lines.append(f"{var['name']} {var['type']} {value_str}")

        self.variables_text.setPlainText("\n".join(lines))

    def _apply_text_changes(self):
        """Parse text and update variables list"""
        text = self.variables_text.toPlainText()
        lines = [line.strip() for line in text.split('\n') if line.strip()]

        new_variables = []
        errors = []

        for i, line in enumerate(lines, 1):
            parts = line.split(None, 2)  # Split on whitespace, max 3 parts

            if len(parts) != 3:
                errors.append(f"Line {i}: Invalid format (expected: name type value)")
                continue

            name, var_type, value_str = parts

            # Validate name
            if not name.isidentifier():
                errors.append(f"Line {i}: Invalid variable name '{name}'")
                continue

            # Validate type
            if var_type not in SUPPORTED_TYPES:
                errors.append(f"Line {i}: Unsupported type '{var_type}'")
                continue

            # Parse value
            value, error = parse_global_value(var_type, value_str)
            if error is not None:
                errors.append(f"Line {i}: Invalid value '{value_str}' for type {var_type}: {error}")
                continue

            new_variables.append({
                "name": name,
                "type": var_type,
                "value": value
            })

        if errors:
            QMessageBox.warning(
                self,
                "Parse Errors",
                "The following errors were found:\n\n" + "\n".join(errors)
            )
            return

        # Update variables
        self.variables = new_variables
        self._refresh_variables_table()
        self._save_globals()
        self.globals_modified.emit()

        QMessageBox.information(
            self,
            "Success",
            f"Successfully parsed {len(new_variables)} variable(s)."
        )

    def _add_singleton(self):
        """Add a new singleton"""
        dialog = AddSingletonDialog(self.project_dir, self)
        if dialog.exec():
            data = dialog.get_data()

            # Check if global name already exists
            if any(s["global_name"] == data["global_name"] for s in self.singletons):
                QMessageBox.warning(
                    self,
                    "Duplicate Global Name",
                    f"A singleton with global name '{data['global_name']}' already exists."
                )
                return

            self.singletons.append({
                "global_name": data["global_name"],
                "script_path": data["script_path"]
            })

            self._refresh_singletons_table()
            self._save_globals()
            self.globals_modified.emit()

    def _remove_singleton(self):
        """Remove selected singleton"""
        current_row = self.singletons_table.currentRow()
        if current_row < 0:
            QMessageBox.information(self, "No Selection", "Please select a singleton to remove.")
            return

        reply = QMessageBox.question(
            self,
            "Confirm Removal",
            "Are you sure you want to remove this singleton?",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
        )

        if reply == QMessageBox.StandardButton.Yes:
            del self.singletons[current_row]
            self._refresh_singletons_table()
            self._save_globals()
            self.globals_modified.emit()

    def _open_singleton_script(self):
        """Open the selected singleton script in script editor"""
        current_row = self.singletons_table.currentRow()
        if current_row < 0:
            QMessageBox.information(self, "No Selection", "Please select a singleton to open.")
            return

        singleton = self.singletons[current_row]
        script_path = os.path.join(self.project_dir, singleton["script_path"])

        # Try to get the script editor panel from parent
        main_editor = self.parent()
        if main_editor and hasattr(main_editor, 'panels'):
            script_editor = main_editor.panels.get('script_editor')
            if script_editor:
                script_editor.open_script(script_path)
                script_editor.raise_()
                script_editor.setFocus()
            else:
                QMessageBox.information(
                    self,
                    "Script Editor Not Available",
                    "Please open the Script Editor panel first."
                )
        else:
            QMessageBox.information(
                self,
                "Cannot Open",
                f"Script path: {script_path}\n\nPlease open manually in Script Editor."
            )

    def _add_variable(self):
        """Add a new variable"""
        dialog = AddVariableDialog(self)
        if dialog.exec():
            data = dialog.get_data()

            # Check if name already exists
            if any(v["name"] == data["name"] for v in self.variables):
                QMessageBox.warning(
                    self,
                    "Duplicate Variable Name",
                    f"A variable named '{data['name']}' already exists."
                )
                return

            self.variables.append(data)
            self._refresh_variables_table()
            self._save_globals()
            self.globals_modified.emit()

    def _edit_variable(self):
        """Edit selected variable"""
        current_row = self.variables_table.currentRow()
        if current_row < 0:
            QMessageBox.information(self, "No Selection", "Please select a variable to edit.")
            return

        # Disable sorting during edit to maintain row index
        self.variables_table.setSortingEnabled(False)

        # Find the actual variable by matching all data
        selected_name = self.variables_table.item(current_row, 0).text()
        selected_type = self.variables_table.item(current_row, 1).text()
        selected_value_str = self.variables_table.item(current_row, 2).text()

        # Find matching variable
        var_index = -1
        for i, var in enumerate(self.variables):
            value_str = format_global_value(var["type"], var["value"])
            if var["name"] == selected_name and var["type"] == selected_type and value_str == selected_value_str:
                var_index = i
                break

        if var_index < 0:
            QMessageBox.warning(self, "Error", "Could not find variable data.")
            self.variables_table.setSortingEnabled(True)
            return

        old_name = self.variables[var_index]["name"]

        dialog = AddVariableDialog(self, edit_data=self.variables[var_index])
        if dialog.exec():
            data = dialog.get_data()

            # Check if new name conflicts with other variables
            if data["name"] != old_name and any(v["name"] == data["name"] for v in self.variables):
                QMessageBox.warning(
                    self,
                    "Duplicate Variable Name",
                    f"A variable named '{data['name']}' already exists."
                )
                self.variables_table.setSortingEnabled(True)
                return

            self.variables[var_index] = data
            self._refresh_variables_table()
            self._save_globals()
            self.globals_modified.emit()

        self.variables_table.setSortingEnabled(True)

    def _remove_variable(self):
        """Remove selected variable"""
        current_row = self.variables_table.currentRow()
        if current_row < 0:
            QMessageBox.information(self, "No Selection", "Please select a variable to remove.")
            return

        # Disable sorting to maintain row index
        self.variables_table.setSortingEnabled(False)

        # Find the actual variable by matching all data
        selected_name = self.variables_table.item(current_row, 0).text()
        selected_type = self.variables_table.item(current_row, 1).text()
        selected_value_str = self.variables_table.item(current_row, 2).text()

        # Find matching variable
        var_index = -1
        for i, var in enumerate(self.variables):
            value_str = format_global_value(var["type"], var["value"])
            if var["name"] == selected_name and var["type"] == selected_type and value_str == selected_value_str:
                var_index = i
                break

        if var_index < 0:
            QMessageBox.warning(self, "Error", "Could not find variable data.")
            self.variables_table.setSortingEnabled(True)
            return

        reply = QMessageBox.question(
            self,
            "Confirm Removal",
            f"Are you sure you want to remove variable '{self.variables[var_index]['name']}'?",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
        )

        if reply == QMessageBox.StandardButton.Yes:
            del self.variables[var_index]
            self._refresh_variables_table()
            self._save_globals()
            self.globals_modified.emit()

        self.variables_table.setSortingEnabled(True)

    def _refresh_singletons_table(self):
        """Refresh the singletons table"""
        self.singletons_table.setRowCount(0)

        for singleton in self.singletons:
            row = self.singletons_table.rowCount()
            self.singletons_table.insertRow(row)

            self.singletons_table.setItem(row, 0, QTableWidgetItem(singleton["global_name"]))
            self.singletons_table.setItem(row, 1, QTableWidgetItem(singleton["script_path"]))

    def _refresh_variables_table(self):
        """Refresh the variables table"""
        self.variables_table.setRowCount(0)

        for var in self.variables:
            row = self.variables_table.rowCount()
            self.variables_table.insertRow(row)

            value_str = format_global_value(var["type"], var["value"])

            self.variables_table.setItem(row, 0, QTableWidgetItem(var["name"]))
            self.variables_table.setItem(row, 1, QTableWidgetItem(var["type"]))
            self.variables_table.setItem(row, 2, QTableWidgetItem(value_str))

    def _save_globals(self):
        """Save globals configuration to file"""
        data = {
            "singletons": self.singletons,
            "variables": self.variables
        }

        try:
            with open(self.globals_file, 'w') as f:
                json.dump(data, f, indent=2)
        except Exception as e:
            QMessageBox.warning(
                self,
                "Save Error",
                f"Failed to save globals configuration: {e}"
            )

    def _load_globals(self):
        """Load globals configuration from file"""
        if not os.path.exists(self.globals_file):
            # Create default empty globals file
            self._save_globals()
            return

        try:
            with open(self.globals_file, 'r') as f:
                data = json.load(f)

            self.singletons = data.get("singletons", [])
            self.variables = data.get("variables", [])

            self._refresh_singletons_table()
            self._refresh_variables_table()
        except Exception as e:
            QMessageBox.warning(
                self,
                "Load Error",
                f"Failed to load globals configuration: {e}"
            )

    def get_globals_data(self):
        """Get the current globals data"""
        return {
            "singletons": self.singletons,
            "variables": self.variables
        }
