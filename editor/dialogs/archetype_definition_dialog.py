"""
Lupine Engine Archetype Definition Dialog
Visual schema editor for creating/editing data-defined archetypes (.archetype).

An archetype definition is the Lupine equivalent of a Unity ScriptableObject class
or a Godot custom Resource: a named set of typed fields plus a Create-menu location.
Concrete data lives in archetype instance assets (.ares).
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

# PropertyHintType enum (from core/include/lupine/core/PropertyDescriptor.hpp)
HINT_TYPES = [
    ("None", 0),
    ("Range", 1),
    ("Enum", 2),
    ("File", 3),
    ("MultilineText", 4),
    ("ExpRange", 5),
    ("Length", 6),
    ("ColorNoAlpha", 7),
    ("Dir", 8),
    ("Layers2D", 9),
    ("Layers3D", 10),
    ("NodeType", 11),
    ("ArchetypeType", 12),
    ("ScriptClass", 13),
    ("Flags", 14),
    ("ExpEasing", 15),
]

# Built-in custom inspector widget ids the dialog offers as presets in the Widget
# column. Authors may still type any other (e.g. project addon) id. The asset widgets
# give drag-and-drop slots, multi-value lists and collapsible image previews.
#   resource / resource_list  - any asset reference (single / multiple)
#   image / image_list        - image reference with inline preview
#   audio / audio_list        - audio reference with a play button
#   archetype / archetype_list- .ares reference (set hint ArchetypeType + class)
KNOWN_WIDGET_IDS = [
    "", "resource", "resource_list", "image", "image_list",
    "audio", "audio_list", "archetype", "archetype_list",
    "slider", "password",
]


# PropertyUsageFlags bitmask (from core/include/lupine/core/PropertyDescriptor.hpp),
# authored as comma-separated tokens in the dialog.
USAGE_FLAGS = [
    ("readonly", 1 << 0),
    ("hidden", 1 << 1),
    ("noserialize", 1 << 2),
    ("required", 1 << 3),
    ("unique", 1 << 4),
    ("experimental", 1 << 5),
    ("advanced", 1 << 6),
]


def usage_to_text(bits):
    try:
        bits = int(bits or 0)
    except (ValueError, TypeError):
        return ""
    return ",".join(name for name, bit in USAGE_FLAGS if bits & bit)


def text_to_usage(text):
    bits = 0
    for token in str(text or "").replace(" ", "").split(","):
        if not token:
            continue
        for name, bit in USAGE_FLAGS:
            if token.lower() == name:
                bits |= bit
    return bits


class ArchetypeDefinitionDialog(QDialog):
    """Dialog for authoring a data-defined archetype schema (.archetype file)."""

    def __init__(self, parent=None, default_location=None, existing_path=None, schema=None,
                 editor_bridge=None):
        super().__init__(parent)
        self.setWindowTitle("Edit Archetype Definition" if existing_path else "Create Archetype Definition")
        self.setModal(True)
        self.setMinimumSize(960, 640)

        self._default_location = default_location or ""
        self._existing_path = existing_path
        self._editor_bridge = editor_bridge
        self._self_class_name = ""
        if schema and isinstance(schema, dict):
            self._self_class_name = schema.get("archetype_class", "")
        self._setup_ui()
        self._populate_base_combo()

        if schema:
            self._load_schema(schema)

        self._refresh_inherited_fields()

    def _setup_ui(self):
        layout = QVBoxLayout()
        layout.setSpacing(12)

        title_label = QLabel("Archetype Definition")
        title_label.setStyleSheet("font-size: 18px; font-weight: bold;")
        layout.addWidget(title_label)

        form_layout = QFormLayout()
        form_layout.setSpacing(8)

        self.name_edit = QLineEdit()
        self.name_edit.setPlaceholderText("EnemyStats")
        self.name_edit.textChanged.connect(self._validate_inputs)
        form_layout.addRow("Archetype Name:", self.name_edit)

        self.description_edit = QLineEdit()
        self.description_edit.setPlaceholderText("A short description of this archetype")
        form_layout.addRow("Description:", self.description_edit)

        self.menu_edit = QLineEdit()
        self.menu_edit.setPlaceholderText("Gameplay/Enemies")
        self.menu_edit.setText("Archetypes")
        form_layout.addRow("Menu Path:", self.menu_edit)

        self.extends_combo = QComboBox()
        self.extends_combo.addItem("(none)", "")
        self.extends_combo.currentIndexChanged.connect(self._on_base_changed)
        form_layout.addRow("Extends:", self.extends_combo)

        self.abstract_check = QCheckBox("Abstract (base type, hidden from the create-instance menu)")
        form_layout.addRow("", self.abstract_check)

        self.implements_list = QListWidget()
        self.implements_list.setObjectName("archetypeImplementsList")
        self.implements_list.setMaximumHeight(96)
        self.implements_list.setSelectionMode(QAbstractItemView.SelectionMode.NoSelection)
        form_layout.addRow("Implements Interfaces:", self.implements_list)
        self._populate_implements_list()

        icon_layout = QHBoxLayout()
        self.icon_edit = QLineEdit()
        self.icon_edit.setPlaceholderText("res://icons/enemy.png (optional)")
        icon_layout.addWidget(self.icon_edit)
        icon_browse = QPushButton("Browse...")
        icon_browse.setProperty("secondary", True)
        icon_browse.setFixedWidth(80)
        icon_browse.clicked.connect(self._browse_icon)
        icon_layout.addWidget(icon_browse)
        form_layout.addRow("Icon:", icon_layout)

        location_layout = QHBoxLayout()
        self.location_edit = QLineEdit()
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

        self.inherited_label = QLabel("Inherited Fields (read-only)")
        self.inherited_label.setStyleSheet("font-weight: bold;")
        self.inherited_label.setVisible(False)
        layout.addWidget(self.inherited_label)

        self.inherited_table = QTableWidget(0, 4)
        self.inherited_table.setHorizontalHeaderLabels(["Name", "Type", "Default", "Group"])
        self.inherited_table.setEditTriggers(QAbstractItemView.EditTrigger.NoEditTriggers)
        self.inherited_table.setSelectionMode(QAbstractItemView.SelectionMode.NoSelection)
        self.inherited_table.setMaximumHeight(140)
        inherited_header = self.inherited_table.horizontalHeader()
        inherited_header.setSectionResizeMode(0, QHeaderView.ResizeMode.Stretch)
        inherited_header.setSectionResizeMode(1, QHeaderView.ResizeMode.ResizeToContents)
        inherited_header.setSectionResizeMode(2, QHeaderView.ResizeMode.Stretch)
        inherited_header.setSectionResizeMode(3, QHeaderView.ResizeMode.Stretch)
        self.inherited_table.setVisible(False)
        layout.addWidget(self.inherited_table)

        fields_label = QLabel("Own Fields")
        fields_label.setStyleSheet("font-weight: bold;")
        layout.addWidget(fields_label)

        self.fields_table = QTableWidget(0, 10)
        self.fields_table.setHorizontalHeaderLabels(
            ["Name", "Type", "Default", "Hint Type", "Hint String", "Group",
             "Header", "Suffix", "Usage", "Widget"])
        self.fields_table.setSelectionBehavior(QAbstractItemView.SelectionBehavior.SelectRows)
        self.fields_table.setSelectionMode(QAbstractItemView.SelectionMode.SingleSelection)
        header = self.fields_table.horizontalHeader()
        header.setSectionResizeMode(0, QHeaderView.ResizeMode.Stretch)
        header.setSectionResizeMode(1, QHeaderView.ResizeMode.ResizeToContents)
        header.setSectionResizeMode(2, QHeaderView.ResizeMode.Stretch)
        header.setSectionResizeMode(3, QHeaderView.ResizeMode.ResizeToContents)
        header.setSectionResizeMode(4, QHeaderView.ResizeMode.Stretch)
        header.setSectionResizeMode(5, QHeaderView.ResizeMode.Stretch)
        header.setSectionResizeMode(6, QHeaderView.ResizeMode.Stretch)
        header.setSectionResizeMode(7, QHeaderView.ResizeMode.ResizeToContents)
        header.setSectionResizeMode(8, QHeaderView.ResizeMode.ResizeToContents)
        header.setSectionResizeMode(9, QHeaderView.ResizeMode.ResizeToContents)
        self.fields_table.setToolTip(
            "Usage tokens: readonly, hidden, noserialize, required, unique, experimental, advanced\n"
            "Widget: a custom inspector widget id (optional)")
        layout.addWidget(self.fields_table)

        field_buttons = QHBoxLayout()
        add_btn = QPushButton("Add Field")
        add_btn.clicked.connect(lambda: self._add_field_row())
        field_buttons.addWidget(add_btn)
        remove_btn = QPushButton("Remove Field")
        remove_btn.setProperty("secondary", True)
        remove_btn.clicked.connect(self._remove_selected_field)
        field_buttons.addWidget(remove_btn)
        up_btn = QPushButton("Move Up")
        up_btn.setProperty("secondary", True)
        up_btn.clicked.connect(lambda: self._move_selected_field(-1))
        field_buttons.addWidget(up_btn)
        down_btn = QPushButton("Move Down")
        down_btn.setProperty("secondary", True)
        down_btn.clicked.connect(lambda: self._move_selected_field(1))
        field_buttons.addWidget(down_btn)
        field_buttons.addStretch()
        layout.addLayout(field_buttons)

        self.path_preview = QLabel()
        self.path_preview.setWordWrap(True)
        self.path_preview.setStyleSheet("font-style: italic;")
        layout.addWidget(self.path_preview)

        button_layout = QHBoxLayout()
        button_layout.addStretch()
        cancel_button = QPushButton("Cancel")
        cancel_button.setProperty("secondary", True)
        cancel_button.clicked.connect(self.reject)
        button_layout.addWidget(cancel_button)
        self.create_button = QPushButton("Save Archetype")
        self.create_button.setProperty("success", True)
        self.create_button.clicked.connect(self._on_accept)
        self.create_button.setEnabled(False)
        button_layout.addWidget(self.create_button)
        layout.addLayout(button_layout)

        self.setLayout(layout)
        self._validate_inputs()

    def _make_type_combo(self, type_table, selected_value):
        combo = QComboBox()
        for label, value in type_table:
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
        return str(type_value)

    def _populate_base_combo(self):
        if not self._editor_bridge:
            return
        try:
            definitions = json.loads(self._editor_bridge.get_archetype_definitions())
        except Exception:
            definitions = []
        names = sorted(d.get("archetype_class", "") for d in definitions if d.get("archetype_class"))
        for name in names:
            if name and name != self._self_class_name:
                self.extends_combo.addItem(name, name)

    def _populate_implements_list(self):
        self.implements_list.clear()
        names = []
        if self._editor_bridge:
            try:
                definitions = json.loads(self._editor_bridge.get_interface_definitions())
            except Exception:
                definitions = []
            names = sorted(d.get("interface_name", "") for d in definitions if d.get("interface_name"))
        for name in names:
            if not name:
                continue
            item = QListWidgetItem(name)
            item.setFlags(item.flags() | Qt.ItemFlag.ItemIsUserCheckable)
            item.setCheckState(Qt.CheckState.Unchecked)
            self.implements_list.addItem(item)

    def _checked_implements(self):
        names = []
        for index in range(self.implements_list.count()):
            item = self.implements_list.item(index)
            if item.checkState() == Qt.CheckState.Checked:
                names.append(item.text())
        return names

    def _set_checked_implements(self, names):
        wanted = set(names or [])
        present = set()
        for index in range(self.implements_list.count()):
            item = self.implements_list.item(index)
            present.add(item.text())
            item.setCheckState(Qt.CheckState.Checked if item.text() in wanted
                               else Qt.CheckState.Unchecked)
        for name in wanted:
            if name and name not in present:
                item = QListWidgetItem(name)
                item.setFlags(item.flags() | Qt.ItemFlag.ItemIsUserCheckable)
                item.setCheckState(Qt.CheckState.Checked)
                self.implements_list.addItem(item)

    def _on_base_changed(self, index):
        self._refresh_inherited_fields()

    def _refresh_inherited_fields(self):
        base = self.extends_combo.currentData() if hasattr(self, 'extends_combo') else ""
        if not base or not self._editor_bridge:
            self.inherited_table.setRowCount(0)
            self.inherited_label.setVisible(False)
            self.inherited_table.setVisible(False)
            return

        try:
            fields = json.loads(self._editor_bridge.get_archetype_effective_fields(base))
        except Exception:
            fields = []

        self.inherited_table.setRowCount(0)
        for field in fields:
            row = self.inherited_table.rowCount()
            self.inherited_table.insertRow(row)
            self.inherited_table.setItem(row, 0, QTableWidgetItem(field.get("name", "")))
            self.inherited_table.setItem(row, 1, QTableWidgetItem(self._type_label(field.get("type"))))
            self.inherited_table.setItem(
                row, 2, QTableWidgetItem(self._default_to_text(field.get("type"), field.get("default"))))
            self.inherited_table.setItem(row, 3, QTableWidgetItem(field.get("group", "")))

        has_fields = self.inherited_table.rowCount() > 0
        self.inherited_label.setVisible(has_fields)
        self.inherited_table.setVisible(has_fields)

    def _add_field_row(self, name="", type_value=1, default="", hint_value=0, hint_string="",
                       group="", header="", suffix="", usage=0, widget="", widget_config=""):
        row = self.fields_table.rowCount()
        self.fields_table.insertRow(row)

        name_edit = QLineEdit(name)
        name_edit.setPlaceholderText("field_name")
        # Custom-widget config has no dedicated cell; round-trip it on the name cell so
        # it is preserved across load/save without widening the table further.
        name_edit.setProperty("widget_config", widget_config or "")
        self.fields_table.setCellWidget(row, 0, name_edit)

        self.fields_table.setCellWidget(row, 1, self._make_type_combo(PROPERTY_TYPES, type_value))

        default_edit = QLineEdit(default)
        default_edit.setPlaceholderText("default value")
        self.fields_table.setCellWidget(row, 2, default_edit)

        self.fields_table.setCellWidget(row, 3, self._make_type_combo(HINT_TYPES, hint_value))

        hint_edit = QLineEdit(hint_string)
        hint_edit.setPlaceholderText("min,max,step  |  A,B,C  |  *.png  |  ClassName")
        self.fields_table.setCellWidget(row, 4, hint_edit)

        group_edit = QLineEdit(group)
        group_edit.setPlaceholderText("group (optional)")
        self.fields_table.setCellWidget(row, 5, group_edit)

        header_edit = QLineEdit(header)
        header_edit.setPlaceholderText("section header (optional)")
        self.fields_table.setCellWidget(row, 6, header_edit)

        suffix_edit = QLineEdit(suffix)
        suffix_edit.setPlaceholderText("unit")
        self.fields_table.setCellWidget(row, 7, suffix_edit)

        usage_edit = QLineEdit(usage_to_text(usage))
        usage_edit.setPlaceholderText("readonly,required")
        self.fields_table.setCellWidget(row, 8, usage_edit)

        widget_combo = QComboBox()
        widget_combo.setEditable(True)
        widget_combo.addItems(KNOWN_WIDGET_IDS)
        if widget and widget not in KNOWN_WIDGET_IDS:
            widget_combo.addItem(widget)
        widget_combo.setCurrentText(widget or "")
        widget_combo.setToolTip(
            "Custom inspector widget id (optional).\n"
            "Asset presets: resource(_list), image(_list), audio(_list), archetype(_list).\n"
            "Pair *_list widgets with a StringArray field for multiple values.")
        self.fields_table.setCellWidget(row, 9, widget_combo)

    def _read_rows(self):
        rows = []
        for row in range(self.fields_table.rowCount()):
            name_widget = self.fields_table.cellWidget(row, 0)
            name = name_widget.text().strip()
            type_value = self.fields_table.cellWidget(row, 1).currentData()
            default = self.fields_table.cellWidget(row, 2).text()
            hint_value = self.fields_table.cellWidget(row, 3).currentData()
            hint_string = self.fields_table.cellWidget(row, 4).text().strip()
            group = self.fields_table.cellWidget(row, 5).text().strip()
            header = self.fields_table.cellWidget(row, 6).text().strip()
            suffix = self.fields_table.cellWidget(row, 7).text().strip()
            usage = text_to_usage(self.fields_table.cellWidget(row, 8).text())
            widget = self.fields_table.cellWidget(row, 9).currentText().strip()
            widget_config = name_widget.property("widget_config") or ""
            rows.append({
                "name": name,
                "type": type_value,
                "default": default,
                "hint_type": hint_value,
                "hint_string": hint_string,
                "group": group,
                "header": header,
                "suffix": suffix,
                "usage": usage,
                "custom_widget": widget,
                "custom_widget_config": widget_config,
            })
        return rows

    def _set_rows(self, rows):
        self.fields_table.setRowCount(0)
        for row in rows:
            self._add_field_row(
                row.get("name", ""),
                row.get("type", 1),
                row.get("default", ""),
                row.get("hint_type", 0),
                row.get("hint_string", ""),
                row.get("group", ""),
                row.get("header", ""),
                row.get("suffix", ""),
                row.get("usage", 0),
                row.get("custom_widget", ""),
                row.get("custom_widget_config", ""),
            )

    def _remove_selected_field(self):
        row = self.fields_table.currentRow()
        if row >= 0:
            self.fields_table.removeRow(row)

    def _move_selected_field(self, direction):
        row = self.fields_table.currentRow()
        if row < 0:
            return
        rows = self._read_rows()
        target = row + direction
        if target < 0 or target >= len(rows):
            return
        rows[row], rows[target] = rows[target], rows[row]
        self._set_rows(rows)
        self.fields_table.selectRow(target)

    def _browse_icon(self):
        start = self.location_edit.text() or str(Path.home())
        file_path, _ = QFileDialog.getOpenFileName(
            self, "Select Icon", start, "Images (*.png *.jpg *.jpeg *.bmp)")
        if file_path:
            self.icon_edit.setText(file_path)

    def _browse_location(self):
        current = self.location_edit.text() or str(Path.home())
        folder = QFileDialog.getExistingDirectory(
            self, "Select Archetype Save Location", current, QFileDialog.Option.ShowDirsOnly)
        if folder:
            self.location_edit.setText(folder)

    def _validate_inputs(self):
        # textChanged can fire while prefilling fields, before the buttons exist.
        if not hasattr(self, 'create_button'):
            return

        name = self.name_edit.text().strip()
        location = self.location_edit.text().strip()
        self.create_button.setEnabled(bool(name and location))

        if name and location:
            full_path = Path(location) / (name if name.endswith(".archetype") else name + ".archetype")
            if full_path.exists() and not self._existing_path:
                self.path_preview.setText(f"Warning: File already exists: {full_path}")
                self.path_preview.setStyleSheet("color: orange; font-style: italic;")
            else:
                self.path_preview.setText(f"Archetype will be saved at: {full_path}")
                self.path_preview.setStyleSheet("font-style: italic;")
        else:
            self.path_preview.setText("")

    def _encode_default(self, type_value, text):
        text = text.strip()

        def parse_floats(count, fallback):
            parts = [p for p in text.replace(" ", "").split(",") if p != ""]
            values = []
            for part in parts:
                try:
                    values.append(float(part))
                except ValueError:
                    values.append(0.0)
            while len(values) < count:
                values.append(fallback)
            return values[:count]

        if type_value == 0:  # Int
            try:
                return int(text)
            except ValueError:
                return 0
        if type_value == 1:  # Float
            try:
                return float(text)
            except ValueError:
                return 0.0
        if type_value == 3:  # Bool
            return text.lower() in ("true", "1", "yes", "on")
        if type_value == 4:  # Vec2
            v = parse_floats(2, 0.0)
            return {"x": v[0], "y": v[1]}
        if type_value == 5:  # Vec3
            v = parse_floats(3, 0.0)
            return {"x": v[0], "y": v[1], "z": v[2]}
        if type_value == 6:  # Vec4
            v = parse_floats(4, 0.0)
            return {"x": v[0], "y": v[1], "z": v[2], "w": v[3]}
        if type_value == 7:  # Color
            v = parse_floats(4, 1.0)
            return {"r": v[0], "g": v[1], "b": v[2], "a": v[3]}
        if type_value == 10:  # Enum (stored as index)
            try:
                return int(text)
            except ValueError:
                return 0
        if type_value == 11:  # StringArray
            return [p.strip() for p in text.split(",") if p.strip()]
        if type_value == 12:  # Double
            try:
                return float(text)
            except ValueError:
                return 0.0
        if type_value == 13:  # Quat (w, x, y, z) — identity default
            v = parse_floats(4, 0.0)
            if not text:
                return {"w": 1.0, "x": 0.0, "y": 0.0, "z": 0.0}
            return {"w": v[0], "x": v[1], "y": v[2], "z": v[3]}
        if type_value == 14:  # Rect (x, y, w, h)
            v = parse_floats(4, 0.0)
            return {"x": v[0], "y": v[1], "w": v[2], "h": v[3]}
        if type_value == 16:  # IntArray
            result = []
            for p in text.split(","):
                p = p.strip()
                if not p:
                    continue
                try:
                    result.append(int(p))
                except ValueError:
                    pass
            return result
        if type_value == 17:  # FloatArray
            result = []
            for p in text.split(","):
                p = p.strip()
                if not p:
                    continue
                try:
                    result.append(float(p))
                except ValueError:
                    pass
            return result
        if type_value == 18:  # Array (generic) — parsed as JSON, else empty list
            if not text:
                return []
            try:
                parsed = json.loads(text)
                return parsed if isinstance(parsed, list) else []
            except (ValueError, TypeError):
                return []
        if type_value == 19:  # Dictionary (generic) — parsed as JSON, else empty object
            if not text:
                return {}
            try:
                parsed = json.loads(text)
                return parsed if isinstance(parsed, dict) else {}
            except (ValueError, TypeError):
                return {}
        # String / NodePath / ScenePath / Resource
        return text

    def get_save_path(self):
        if self._existing_path:
            return self._existing_path
        name = self.name_edit.text().strip()
        location = self.location_edit.text().strip()
        filename = name if name.endswith(".archetype") else name + ".archetype"
        return str(Path(location) / filename)

    def get_schema_json(self):
        fields = []
        for row in self._read_rows():
            if not row["name"]:
                continue
            field = {
                "name": row["name"],
                "type": row["type"],
                "default": self._encode_default(row["type"], row["default"]),
                "hint": {
                    "type": row["hint_type"],
                    "hint_string": row["hint_string"],
                },
                "description": "",
                "group": row["group"],
            }
            # Extended editor metadata - emitted only when set (matches the C++
            # PropertyDescriptor::Serialize convention so files stay lean).
            if row.get("header"):
                field["header"] = row["header"]
            if row.get("suffix"):
                field["suffix"] = row["suffix"]
            if row.get("usage"):
                field["usage"] = row["usage"]
            if row.get("custom_widget"):
                field["custom_widget"] = row["custom_widget"]
            if row.get("custom_widget_config"):
                field["custom_widget_config"] = row["custom_widget_config"]
            fields.append(field)

        return {
            "lupine_archetype": 1,
            "archetype_class": self.name_edit.text().strip(),
            "extends": self.extends_combo.currentData() or "",
            "abstract": self.abstract_check.isChecked(),
            "menu_path": self.menu_edit.text().strip() or "Archetypes",
            "icon": self.icon_edit.text().strip(),
            "description": self.description_edit.text().strip(),
            "implements": self._checked_implements(),
            "fields": fields,
        }

    def _load_schema(self, schema):
        if not isinstance(schema, dict):
            return
        self.name_edit.setText(schema.get("archetype_class", ""))
        self.description_edit.setText(schema.get("description", ""))
        self.menu_edit.setText(schema.get("menu_path", "Archetypes"))
        self.icon_edit.setText(schema.get("icon", ""))

        base = schema.get("extends", "")
        if base:
            index = self.extends_combo.findData(base)
            if index < 0:
                self.extends_combo.addItem(base, base)
                index = self.extends_combo.findData(base)
            self.extends_combo.setCurrentIndex(index)
        self.abstract_check.setChecked(bool(schema.get("abstract", False)))

        implements = schema.get("implements", [])
        if isinstance(implements, str):
            implements = [implements] if implements else []
        self._set_checked_implements(implements)

        rows = []
        for field in schema.get("fields", []):
            type_value = field.get("type", 1)
            rows.append({
                "name": field.get("name", ""),
                "type": type_value,
                "default": self._default_to_text(type_value, field.get("default")),
                "hint_type": self._normalize_hint(field.get("hint", {})).get("type", 0),
                "hint_string": self._normalize_hint(field.get("hint", {})).get("hint_string", ""),
                "group": field.get("group", ""),
                "header": field.get("header", ""),
                "suffix": field.get("suffix", ""),
                "usage": field.get("usage", 0),
                "custom_widget": field.get("custom_widget", ""),
                "custom_widget_config": field.get("custom_widget_config", ""),
            })
        self._set_rows(rows)

    @staticmethod
    def _normalize_hint(hint) -> dict:
        """Coerce legacy string hints to the canonical dict form."""
        if isinstance(hint, dict):
            return hint
        _STRING_TO_TYPE = {"file": 3, "multiline": 4, "dir": 8}
        if isinstance(hint, str):
            hint_type = _STRING_TO_TYPE.get(hint.lower(), 0)
            hint_string = "" if hint_type != 0 else hint
            return {"type": hint_type, "hint_string": hint_string}
        return {"type": 0, "hint_string": ""}

    def _default_to_text(self, type_value, value):
        if value is None:
            return ""
        if type_value in (4, 5, 6):  # Vec2/3/4
            keys = ["x", "y", "z", "w"][:({4: 2, 5: 3, 6: 4}[type_value])]
            return ",".join(str(value.get(k, 0.0)) for k in keys) if isinstance(value, dict) else ""
        if type_value == 7:  # Color
            return ",".join(str(value.get(k, 0.0)) for k in ("r", "g", "b", "a")) if isinstance(value, dict) else ""
        if type_value == 13:  # Quat (w, x, y, z)
            return ",".join(str(value.get(k, 0.0)) for k in ("w", "x", "y", "z")) if isinstance(value, dict) else ""
        if type_value == 14:  # Rect (x, y, w, h)
            return ",".join(str(value.get(k, 0.0)) for k in ("x", "y", "w", "h")) if isinstance(value, dict) else ""
        if type_value in (11, 16, 17):  # StringArray / IntArray / FloatArray
            return ",".join(str(v) for v in value) if isinstance(value, list) else ""
        if type_value in (18, 19):  # Array / Dictionary (generic JSON)
            try:
                return json.dumps(value)
            except (TypeError, ValueError):
                return ""
        return str(value)

    def _on_accept(self):
        name = self.name_edit.text().strip()
        if not name:
            QMessageBox.warning(self, "Invalid Name", "Archetype name is required.")
            return
        if not name[0].isalpha() and name[0] != "_":
            QMessageBox.warning(self, "Invalid Name",
                                "Archetype name must start with a letter or underscore.")
            return

        seen = set()
        for row in self._read_rows():
            if not row["name"]:
                continue
            if row["name"] in seen:
                QMessageBox.warning(self, "Duplicate Field",
                                    f"Field '{row['name']}' is defined more than once.")
                return
            seen.add(row["name"])

        self.accept()
