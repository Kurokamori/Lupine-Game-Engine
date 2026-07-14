"""
Lupine Engine Interface Definition Dialog
Visual schema editor for creating/editing data-defined interfaces (.interface).

An interface is a named capability contract: a set of required methods plus
required signals. Scripts, archetypes, and native components declare that they
implement an interface; the engine indexes those declarations so the running
game and the editor can query "every Damageable" and verify the contract.
"""

from PyQt6.QtWidgets import (QDialog, QVBoxLayout, QHBoxLayout, QLabel,
                             QLineEdit, QPushButton, QFileDialog, QComboBox,
                             QFormLayout, QMessageBox, QTableWidget,
                             QTableWidgetItem, QHeaderView, QAbstractItemView,
                             QCheckBox, QListWidget, QListWidgetItem)
from PyQt6.QtCore import Qt
from pathlib import Path
import json


# PropertyValueType enum (from core/include/lupine/core/PropertyDescriptor.hpp)
PROPERTY_TYPES = [
    ("Int", 0),
    ("Float", 1),
    ("String", 2),
    ("Bool", 3),
    ("Vec2", 4),
    ("Vec3", 5),
    ("Vec4", 6),
    ("Color", 7),
    ("NodePath", 8),
    ("ScenePath", 9),
    ("Enum", 10),
    ("StringArray", 11),
    ("Double", 12),
    ("Quat", 13),
    ("Rect", 14),
    ("Resource", 15),
    ("IntArray", 16),
    ("FloatArray", 17),
    ("Array", 18),
    ("Dictionary", 19),
]


class InterfaceDefinitionDialog(QDialog):
    """Dialog for authoring a data-defined interface schema (.interface file)."""

    def __init__(self, parent=None, default_location=None, existing_path=None, schema=None,
                 editor_bridge=None):
        super().__init__(parent)
        self.setWindowTitle("Edit Interface Definition" if existing_path else "Create Interface Definition")
        self.setModal(True)
        self.setMinimumSize(760, 680)

        self._default_location = default_location or ""
        self._existing_path = existing_path
        self._editor_bridge = editor_bridge
        self._self_name = ""
        if schema and isinstance(schema, dict):
            self._self_name = schema.get("interface_name", "")
        self._setup_ui()
        self._populate_extends_list()

        if schema:
            self._load_schema(schema)

    # ------------------------------------------------------------------
    # UI construction
    # ------------------------------------------------------------------

    def _setup_ui(self):
        layout = QVBoxLayout()
        layout.setSpacing(12)

        title_label = QLabel("Interface Definition")
        title_label.setObjectName("interfaceDialogTitle")
        title_label.setStyleSheet("font-size: 18px; font-weight: bold;")
        layout.addWidget(title_label)

        form_layout = QFormLayout()
        form_layout.setSpacing(8)

        self.name_edit = QLineEdit()
        self.name_edit.setObjectName("interfaceNameEdit")
        self.name_edit.setPlaceholderText("Damageable")
        self.name_edit.textChanged.connect(self._validate_inputs)
        form_layout.addRow("Interface Name:", self.name_edit)

        self.description_edit = QLineEdit()
        self.description_edit.setObjectName("interfaceDescriptionEdit")
        self.description_edit.setPlaceholderText("A short description of this interface")
        form_layout.addRow("Description:", self.description_edit)

        self.tags_edit = QLineEdit()
        self.tags_edit.setObjectName("interfaceTagsEdit")
        self.tags_edit.setPlaceholderText("combat, gameplay (comma separated)")
        form_layout.addRow("Tags:", self.tags_edit)

        location_layout = QHBoxLayout()
        self.location_edit = QLineEdit()
        self.location_edit.setObjectName("interfaceLocationEdit")
        self.location_edit.setPlaceholderText("Select save location...")
        if self._existing_path:
            self.location_edit.setText(str(Path(self._existing_path).parent))
            self.name_edit.setText(Path(self._existing_path).stem)
        elif self._default_location:
            self.location_edit.setText(self._default_location)
        self.location_edit.textChanged.connect(self._validate_inputs)
        location_layout.addWidget(self.location_edit)
        loc_browse = QPushButton("Browse...")
        loc_browse.setProperty("secondary", True)
        loc_browse.setFixedWidth(80)
        loc_browse.clicked.connect(self._browse_location)
        location_layout.addWidget(loc_browse)
        form_layout.addRow("Location:", location_layout)

        layout.addLayout(form_layout)

        extends_label = QLabel("Extends (base interfaces)")
        extends_label.setStyleSheet("font-weight: bold;")
        layout.addWidget(extends_label)

        self.extends_list = QListWidget()
        self.extends_list.setObjectName("interfaceExtendsList")
        self.extends_list.setMaximumHeight(96)
        self.extends_list.setSelectionMode(QAbstractItemView.SelectionMode.NoSelection)
        layout.addWidget(self.extends_list)

        methods_label = QLabel("Required Methods")
        methods_label.setStyleSheet("font-weight: bold;")
        layout.addWidget(methods_label)

        self.methods_table = QTableWidget(0, 5)
        self.methods_table.setObjectName("interfaceMethodsTable")
        self.methods_table.setHorizontalHeaderLabels(
            ["Name", "Params (name:Type, ...)", "Returns", "Return Type", "Doc"])
        self.methods_table.setSelectionBehavior(QAbstractItemView.SelectionBehavior.SelectRows)
        self.methods_table.setSelectionMode(QAbstractItemView.SelectionMode.SingleSelection)
        m_header = self.methods_table.horizontalHeader()
        m_header.setSectionResizeMode(0, QHeaderView.ResizeMode.ResizeToContents)
        m_header.setSectionResizeMode(1, QHeaderView.ResizeMode.Stretch)
        m_header.setSectionResizeMode(2, QHeaderView.ResizeMode.ResizeToContents)
        m_header.setSectionResizeMode(3, QHeaderView.ResizeMode.ResizeToContents)
        m_header.setSectionResizeMode(4, QHeaderView.ResizeMode.Stretch)
        layout.addWidget(self.methods_table)

        method_buttons = QHBoxLayout()
        add_method_btn = QPushButton("Add Method")
        add_method_btn.clicked.connect(lambda: self._add_method_row())
        method_buttons.addWidget(add_method_btn)
        remove_method_btn = QPushButton("Remove Method")
        remove_method_btn.setProperty("secondary", True)
        remove_method_btn.clicked.connect(self._remove_selected_method)
        method_buttons.addWidget(remove_method_btn)
        up_method_btn = QPushButton("Move Up")
        up_method_btn.setProperty("secondary", True)
        up_method_btn.clicked.connect(lambda: self._move_selected_method(-1))
        method_buttons.addWidget(up_method_btn)
        down_method_btn = QPushButton("Move Down")
        down_method_btn.setProperty("secondary", True)
        down_method_btn.clicked.connect(lambda: self._move_selected_method(1))
        method_buttons.addWidget(down_method_btn)
        method_buttons.addStretch()
        layout.addLayout(method_buttons)

        signals_label = QLabel("Required Signals")
        signals_label.setStyleSheet("font-weight: bold;")
        layout.addWidget(signals_label)

        self.signals_table = QTableWidget(0, 3)
        self.signals_table.setObjectName("interfaceSignalsTable")
        self.signals_table.setHorizontalHeaderLabels(
            ["Name", "Args (name:Type, ...)", "Doc"])
        self.signals_table.setSelectionBehavior(QAbstractItemView.SelectionBehavior.SelectRows)
        self.signals_table.setSelectionMode(QAbstractItemView.SelectionMode.SingleSelection)
        s_header = self.signals_table.horizontalHeader()
        s_header.setSectionResizeMode(0, QHeaderView.ResizeMode.ResizeToContents)
        s_header.setSectionResizeMode(1, QHeaderView.ResizeMode.Stretch)
        s_header.setSectionResizeMode(2, QHeaderView.ResizeMode.Stretch)
        layout.addWidget(self.signals_table)

        signal_buttons = QHBoxLayout()
        add_signal_btn = QPushButton("Add Signal")
        add_signal_btn.clicked.connect(lambda: self._add_signal_row())
        signal_buttons.addWidget(add_signal_btn)
        remove_signal_btn = QPushButton("Remove Signal")
        remove_signal_btn.setProperty("secondary", True)
        remove_signal_btn.clicked.connect(self._remove_selected_signal)
        signal_buttons.addWidget(remove_signal_btn)
        signal_buttons.addStretch()
        layout.addLayout(signal_buttons)

        self.path_preview = QLabel()
        self.path_preview.setObjectName("interfacePathPreview")
        self.path_preview.setWordWrap(True)
        self.path_preview.setStyleSheet("font-style: italic;")
        layout.addWidget(self.path_preview)

        button_layout = QHBoxLayout()
        button_layout.addStretch()
        cancel_button = QPushButton("Cancel")
        cancel_button.setProperty("secondary", True)
        cancel_button.clicked.connect(self.reject)
        button_layout.addWidget(cancel_button)
        self.create_button = QPushButton("Save Interface")
        self.create_button.setObjectName("interfaceSaveButton")
        self.create_button.setProperty("success", True)
        self.create_button.clicked.connect(self._on_accept)
        self.create_button.setEnabled(False)
        button_layout.addWidget(self.create_button)
        layout.addLayout(button_layout)

        self.setLayout(layout)
        self._validate_inputs()

    # ------------------------------------------------------------------
    # Type helpers
    # ------------------------------------------------------------------

    def _make_type_combo(self, selected_value):
        combo = QComboBox()
        for label, value in PROPERTY_TYPES:
            combo.addItem(label, value)
        for index in range(combo.count()):
            if combo.itemData(index) == selected_value:
                combo.setCurrentIndex(index)
                break
        return combo

    def _type_label(self, type_value):
        for label, value in PROPERTY_TYPES:
            if value == type_value:
                return label
        return "Float"

    def _type_value(self, label):
        target = (label or "").strip().lower()
        for name, value in PROPERTY_TYPES:
            if name.lower() == target:
                return value
        return 1

    def _parse_param_text(self, text):
        params = []
        for segment in (text or "").split(","):
            segment = segment.strip()
            if not segment:
                continue
            if ":" in segment:
                name, type_label = segment.split(":", 1)
                name = name.strip()
                type_value = self._type_value(type_label)
            else:
                name = segment
                type_value = 1
            if name:
                params.append({"name": name, "type": type_value})
        return params

    def _format_param_list(self, params):
        if not isinstance(params, list):
            return ""
        parts = []
        for param in params:
            if isinstance(param, dict):
                parts.append(f"{param.get('name', '')}:{self._type_label(param.get('type', 1))}")
        return ", ".join(parts)

    # ------------------------------------------------------------------
    # Extends list
    # ------------------------------------------------------------------

    def _populate_extends_list(self):
        self.extends_list.clear()
        names = []
        if self._editor_bridge:
            try:
                definitions = json.loads(self._editor_bridge.get_interface_definitions())
            except Exception:
                definitions = []
            names = sorted(d.get("interface_name", "") for d in definitions if d.get("interface_name"))
        for name in names:
            if not name or name == self._self_name:
                continue
            item = QListWidgetItem(name)
            item.setFlags(item.flags() | Qt.ItemFlag.ItemIsUserCheckable)
            item.setCheckState(Qt.CheckState.Unchecked)
            self.extends_list.addItem(item)

    def _checked_extends(self):
        names = []
        for index in range(self.extends_list.count()):
            item = self.extends_list.item(index)
            if item.checkState() == Qt.CheckState.Checked:
                names.append(item.text())
        return names

    def _set_checked_extends(self, names):
        wanted = set(names or [])
        present = set()
        for index in range(self.extends_list.count()):
            item = self.extends_list.item(index)
            present.add(item.text())
            item.setCheckState(Qt.CheckState.Checked if item.text() in wanted
                               else Qt.CheckState.Unchecked)
        # Add (checked) entries for base interfaces not currently in the registry list.
        for name in wanted:
            if name and name not in present and name != self._self_name:
                item = QListWidgetItem(name)
                item.setFlags(item.flags() | Qt.ItemFlag.ItemIsUserCheckable)
                item.setCheckState(Qt.CheckState.Checked)
                self.extends_list.addItem(item)

    # ------------------------------------------------------------------
    # Methods table
    # ------------------------------------------------------------------

    def _add_method_row(self, name="", params=None, has_return=False, return_type=1, doc=""):
        row = self.methods_table.rowCount()
        self.methods_table.insertRow(row)

        name_edit = QLineEdit(name)
        name_edit.setPlaceholderText("take_damage")
        self.methods_table.setCellWidget(row, 0, name_edit)

        params_edit = QLineEdit(self._format_param_list(params) if params else "")
        params_edit.setPlaceholderText("amount:Float, source:NodePath")
        self.methods_table.setCellWidget(row, 1, params_edit)

        returns_check = QCheckBox()
        returns_check.setChecked(bool(has_return))
        self.methods_table.setCellWidget(row, 2, returns_check)

        self.methods_table.setCellWidget(row, 3, self._make_type_combo(return_type))

        doc_edit = QLineEdit(doc)
        doc_edit.setPlaceholderText("description (optional)")
        self.methods_table.setCellWidget(row, 4, doc_edit)

    def _read_method_rows(self):
        rows = []
        for row in range(self.methods_table.rowCount()):
            name = self.methods_table.cellWidget(row, 0).text().strip()
            params_text = self.methods_table.cellWidget(row, 1).text()
            has_return = self.methods_table.cellWidget(row, 2).isChecked()
            return_type = self.methods_table.cellWidget(row, 3).currentData()
            doc = self.methods_table.cellWidget(row, 4).text().strip()
            rows.append({
                "name": name,
                "params": self._parse_param_text(params_text),
                "has_return": has_return,
                "return_type": return_type,
                "doc": doc,
            })
        return rows

    def _set_method_rows(self, rows):
        self.methods_table.setRowCount(0)
        for row in rows:
            self._add_method_row(
                row.get("name", ""),
                row.get("params", []),
                row.get("has_return", False),
                row.get("return_type", 1),
                row.get("doc", ""),
            )

    def _remove_selected_method(self):
        row = self.methods_table.currentRow()
        if row >= 0:
            self.methods_table.removeRow(row)

    def _move_selected_method(self, direction):
        row = self.methods_table.currentRow()
        if row < 0:
            return
        rows = self._read_method_rows()
        target = row + direction
        if target < 0 or target >= len(rows):
            return
        rows[row], rows[target] = rows[target], rows[row]
        self._set_method_rows(rows)
        self.methods_table.selectRow(target)

    # ------------------------------------------------------------------
    # Signals table
    # ------------------------------------------------------------------

    def _add_signal_row(self, name="", args=None, doc=""):
        row = self.signals_table.rowCount()
        self.signals_table.insertRow(row)

        name_edit = QLineEdit(name)
        name_edit.setPlaceholderText("died")
        self.signals_table.setCellWidget(row, 0, name_edit)

        args_edit = QLineEdit(self._format_param_list(args) if args else "")
        args_edit.setPlaceholderText("hp:Float")
        self.signals_table.setCellWidget(row, 1, args_edit)

        doc_edit = QLineEdit(doc)
        doc_edit.setPlaceholderText("description (optional)")
        self.signals_table.setCellWidget(row, 2, doc_edit)

    def _read_signal_rows(self):
        rows = []
        for row in range(self.signals_table.rowCount()):
            name = self.signals_table.cellWidget(row, 0).text().strip()
            args_text = self.signals_table.cellWidget(row, 1).text()
            doc = self.signals_table.cellWidget(row, 2).text().strip()
            rows.append({
                "name": name,
                "args": self._parse_param_text(args_text),
                "doc": doc,
            })
        return rows

    def _set_signal_rows(self, rows):
        self.signals_table.setRowCount(0)
        for row in rows:
            self._add_signal_row(row.get("name", ""), row.get("args", []), row.get("doc", ""))

    def _remove_selected_signal(self):
        row = self.signals_table.currentRow()
        if row >= 0:
            self.signals_table.removeRow(row)

    # ------------------------------------------------------------------
    # Location / validation
    # ------------------------------------------------------------------

    def _browse_location(self):
        current = self.location_edit.text() or str(Path.home())
        folder = QFileDialog.getExistingDirectory(
            self, "Select Interface Save Location", current, QFileDialog.Option.ShowDirsOnly)
        if folder:
            self.location_edit.setText(folder)

    def _validate_inputs(self):
        if not hasattr(self, 'create_button'):
            return
        name = self.name_edit.text().strip()
        location = self.location_edit.text().strip()
        self.create_button.setEnabled(bool(name and location))

        if name and location:
            filename = name if name.endswith(".interface") else name + ".interface"
            full_path = Path(location) / filename
            if full_path.exists() and not self._existing_path:
                self.path_preview.setText(f"Warning: File already exists: {full_path}")
                self.path_preview.setStyleSheet("color: orange; font-style: italic;")
            else:
                self.path_preview.setText(f"Interface will be saved at: {full_path}")
                self.path_preview.setStyleSheet("font-style: italic;")
        else:
            self.path_preview.setText("")

    # ------------------------------------------------------------------
    # Schema build / load
    # ------------------------------------------------------------------

    def get_save_path(self):
        if self._existing_path:
            return self._existing_path
        name = self.name_edit.text().strip()
        location = self.location_edit.text().strip()
        filename = name if name.endswith(".interface") else name + ".interface"
        return str(Path(location) / filename)

    def get_schema_json(self):
        methods = []
        for row in self._read_method_rows():
            if not row["name"]:
                continue
            methods.append({
                "name": row["name"],
                "params": row["params"],
                "has_return": row["has_return"],
                "return_type": row["return_type"],
                "doc": row["doc"],
            })

        signals = []
        for row in self._read_signal_rows():
            if not row["name"]:
                continue
            signals.append({
                "name": row["name"],
                "args": row["args"],
                "doc": row["doc"],
            })

        tags = [t.strip() for t in self.tags_edit.text().split(",") if t.strip()]

        return {
            "lupine_interface": 1,
            "interface_name": self.name_edit.text().strip(),
            "description": self.description_edit.text().strip(),
            "extends": self._checked_extends(),
            "methods": methods,
            "signals": signals,
            "tags": tags,
        }

    def _load_schema(self, schema):
        if not isinstance(schema, dict):
            return
        self.name_edit.setText(schema.get("interface_name", ""))
        self.description_edit.setText(schema.get("description", ""))
        self.tags_edit.setText(", ".join(schema.get("tags", []) or []))

        extends = schema.get("extends", [])
        if isinstance(extends, str):
            extends = [extends] if extends else []
        self._set_checked_extends(extends)

        self._set_method_rows(schema.get("methods", []) or [])
        self._set_signal_rows(schema.get("signals", []) or [])

    def _on_accept(self):
        name = self.name_edit.text().strip()
        if not name:
            QMessageBox.warning(self, "Invalid Name", "Interface name is required.")
            return
        if not name[0].isalpha() and name[0] != "_":
            QMessageBox.warning(self, "Invalid Name",
                                "Interface name must start with a letter or underscore.")
            return

        seen = set()
        for row in self._read_method_rows():
            if not row["name"]:
                continue
            if row["name"] in seen:
                QMessageBox.warning(self, "Duplicate Method",
                                    f"Method '{row['name']}' is defined more than once.")
                return
            seen.add(row["name"])

        seen_signals = set()
        for row in self._read_signal_rows():
            if not row["name"]:
                continue
            if row["name"] in seen_signals:
                QMessageBox.warning(self, "Duplicate Signal",
                                    f"Signal '{row['name']}' is defined more than once.")
                return
            seen_signals.add(row["name"])

        self.accept()
