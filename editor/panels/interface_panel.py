"""
Interface Panel

Dock that browses the project's interface definitions (capability contracts:
required methods + required signals). Selecting an interface shows its methods,
signals, tags, base interfaces, and everything that implements it (nodes in the
current scene, archetypes, and native component types). Interfaces are authored
and edited through the InterfaceDefinitionDialog (.interface files).
"""

import json
import os
from pathlib import Path

from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QTreeWidget, QTreeWidgetItem,
    QPushButton, QLabel, QMessageBox
)
from PyQt6.QtCore import Qt

from panels.base_panel import EditorPanel

# Item roles
ROLE_KIND = Qt.ItemDataRole.UserRole          # "interface" | "group" | "member"
ROLE_DATA = Qt.ItemDataRole.UserRole + 1       # dict payload

# PropertyValueType labels (from core/include/lupine/core/PropertyDescriptor.hpp)
_TYPE_LABELS = {
    0: "Int", 1: "Float", 2: "String", 3: "Bool", 4: "Vec2", 5: "Vec3",
    6: "Vec4", 7: "Color", 8: "NodePath", 9: "ScenePath", 10: "Enum",
    11: "StringArray", 12: "Double", 13: "Quat", 14: "Rect", 15: "Resource",
    16: "IntArray", 17: "FloatArray", 18: "Array", 19: "Dictionary",
}


class InterfacePanel(EditorPanel):
    """Browse, author, and inspect interface definitions and their implementers."""

    def __init__(self, parent=None):
        self.editor_bridge = None
        self.main_editor = None
        super().__init__("Interfaces", parent)

    def _setup_panel(self):
        container = QWidget()
        layout = QVBoxLayout(container)
        layout.setContentsMargins(6, 6, 6, 6)

        self._info_label = QLabel("Interface definitions")
        self._info_label.setObjectName("interfacePanelInfoLabel")
        self._info_label.setWordWrap(True)
        layout.addWidget(self._info_label)

        self._tree = QTreeWidget()
        self._tree.setObjectName("interfacePanelTree")
        self._tree.setHeaderLabels(["Interface"])
        self._tree.setColumnCount(1)
        self._tree.itemSelectionChanged.connect(self._on_selection_changed)
        self._tree.itemDoubleClicked.connect(self._on_item_double_clicked)
        layout.addWidget(self._tree, 1)

        button_row = QHBoxLayout()
        self._new_btn = QPushButton("New Interface…")
        self._new_btn.setObjectName("interfacePanelNewButton")
        self._new_btn.clicked.connect(self._on_new)
        self._edit_btn = QPushButton("Edit")
        self._edit_btn.setObjectName("interfacePanelEditButton")
        self._edit_btn.clicked.connect(self._on_edit)
        self._edit_btn.setEnabled(False)
        self._delete_btn = QPushButton("Delete")
        self._delete_btn.setObjectName("interfacePanelDeleteButton")
        self._delete_btn.setProperty("danger", True)
        self._delete_btn.clicked.connect(self._on_delete)
        self._delete_btn.setEnabled(False)
        self._refresh_btn = QPushButton("Refresh")
        self._refresh_btn.setObjectName("interfacePanelRefreshButton")
        self._refresh_btn.clicked.connect(self.refresh)
        button_row.addWidget(self._new_btn)
        button_row.addWidget(self._edit_btn)
        button_row.addWidget(self._delete_btn)
        button_row.addStretch()
        button_row.addWidget(self._refresh_btn)
        layout.addLayout(button_row)

        self.content_layout.addWidget(container)

    # ------------------------------------------------------------------
    # Population
    # ------------------------------------------------------------------

    def refresh(self):
        self._tree.clear()
        if not self.editor_bridge:
            self._info_label.setText("No project loaded.")
            self._update_buttons()
            return

        definitions = self._load_json(self.editor_bridge.get_interface_definitions())
        self._info_label.setText(f"{len(definitions)} interface(s) defined")

        for definition in sorted(definitions, key=lambda d: d.get("interface_name", "")):
            name = definition.get("interface_name", "")
            if not name:
                continue
            source = definition.get("source", "data")
            counts = f"{definition.get('method_count', 0)} methods, {definition.get('signal_count', 0)} signals"
            label = f"{name}  ·  {counts}  ·  {source}"
            item = QTreeWidgetItem([label])
            item.setData(0, ROLE_KIND, "interface")
            item.setData(0, ROLE_DATA, definition)
            self._tree.addTopLevelItem(item)

        self._update_buttons()

    def _on_selection_changed(self):
        self._update_buttons()
        item = self._current_interface_item()
        if not item:
            return
        # Rebuild this interface's detail children on selection.
        item.takeChildren()
        definition = item.data(0, ROLE_DATA) or {}
        name = definition.get("interface_name", "")
        if not name or not self.editor_bridge:
            return

        full = self._load_obj(self.editor_bridge.get_interface_definition(name))
        implementers = self._load_obj(self.editor_bridge.get_interface_implementers(name))

        description = (full or {}).get("description", "")
        if description:
            self._add_group(item, f"Description: {description}", [])

        extends = (full or {}).get("extends", []) or []
        if extends:
            self._add_group(item, "Extends", [str(e) for e in extends])

        method_labels = []
        for method in (full or {}).get("methods", []) or []:
            params = ", ".join(self._format_arg(a) for a in method.get("params", []) or [])
            ret = " -> " + _TYPE_LABELS.get(method.get("return_type", 1), "Float") \
                if method.get("has_return") else ""
            method_labels.append(f"{method.get('name', '')}({params}){ret}")
        self._add_group(item, f"Methods ({len(method_labels)})", method_labels)

        signal_labels = []
        for sig in (full or {}).get("signals", []) or []:
            args = ", ".join(self._format_arg(a) for a in sig.get("args", []) or [])
            signal_labels.append(f"{sig.get('name', '')}({args})")
        self._add_group(item, f"Signals ({len(signal_labels)})", signal_labels)

        tags = (full or {}).get("tags", []) or []
        if tags:
            self._add_group(item, "Tags", [str(t) for t in tags])

        if implementers:
            self._add_group(item, "Nodes (current scene)", implementers.get("nodes", []) or [])
            self._add_group(item, "Archetypes", implementers.get("archetypes", []) or [])
            self._add_group(item, "Component Types", implementers.get("component_types", []) or [])

        item.setExpanded(True)

    def _add_group(self, parent_item, title, members):
        group_item = QTreeWidgetItem([title])
        group_item.setData(0, ROLE_KIND, "group")
        parent_item.addChild(group_item)
        for member in members:
            member_item = QTreeWidgetItem([str(member)])
            member_item.setData(0, ROLE_KIND, "member")
            group_item.addChild(member_item)
        group_item.setExpanded(True)
        return group_item

    @staticmethod
    def _format_arg(arg):
        if not isinstance(arg, dict):
            return str(arg)
        type_label = _TYPE_LABELS.get(arg.get("type", 1), "Float")
        return f"{arg.get('name', '')}:{type_label}"

    # ------------------------------------------------------------------
    # Actions
    # ------------------------------------------------------------------

    def _on_new(self):
        if not self.editor_bridge:
            return
        from dialogs import InterfaceDefinitionDialog
        from PyQt6.QtWidgets import QDialog

        dialog = InterfaceDefinitionDialog(
            self, default_location=self._project_dir(), editor_bridge=self.editor_bridge)
        if dialog.exec() != QDialog.DialogCode.Accepted:
            return
        self._save_definition(dialog)

    def _on_edit(self):
        item = self._current_interface_item()
        if not item or not self.editor_bridge:
            return
        definition = item.data(0, ROLE_DATA) or {}
        source_path = self._resolve_path(definition.get("source_path", ""))
        if not source_path or not source_path.endswith(".interface"):
            QMessageBox.information(
                self, "Cannot Edit",
                "Only data-defined .interface files can be edited here. Script-defined "
                "interfaces are edited in their source script.")
            return

        from dialogs import InterfaceDefinitionDialog
        from PyQt6.QtWidgets import QDialog

        schema = None
        try:
            loaded = self.editor_bridge.load_interface_definition_file(source_path)
            if loaded:
                schema = json.loads(loaded)
        except Exception:
            schema = None

        dialog = InterfaceDefinitionDialog(
            self, existing_path=source_path, schema=schema, editor_bridge=self.editor_bridge)
        if dialog.exec() != QDialog.DialogCode.Accepted:
            return
        self._save_definition(dialog)

    def _on_delete(self):
        item = self._current_interface_item()
        if not item or not self.editor_bridge:
            return
        definition = item.data(0, ROLE_DATA) or {}
        name = definition.get("interface_name", "")
        source_path = self._resolve_path(definition.get("source_path", ""))
        if not source_path or not source_path.endswith(".interface"):
            QMessageBox.information(
                self, "Cannot Delete",
                "Only data-defined .interface files can be deleted here.")
            return

        confirm = QMessageBox.question(
            self, "Delete Interface",
            f"Delete interface '{name}' ({source_path})?",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No)
        if confirm != QMessageBox.StandardButton.Yes:
            return

        try:
            if os.path.exists(source_path):
                os.remove(source_path)
        except OSError as e:
            QMessageBox.warning(self, "Error", f"Failed to delete interface:\n{str(e)}")
            return

        try:
            self.editor_bridge.rescan_interfaces()
        except Exception:
            pass
        self.refresh()

    def _save_definition(self, dialog):
        save_path = dialog.get_save_path()
        schema = dialog.get_schema_json()
        try:
            created = self.editor_bridge.create_interface_definition(save_path, json.dumps(schema))
        except Exception as e:
            QMessageBox.warning(self, "Error", f"Failed to save interface definition:\n{str(e)}")
            return
        if not created:
            QMessageBox.warning(self, "Error", "Failed to write interface definition.")
            return
        self.refresh()

    # ------------------------------------------------------------------
    # Helpers
    # ------------------------------------------------------------------

    def _on_item_double_clicked(self, item, _column):
        if item and item.data(0, ROLE_KIND) == "interface":
            self._on_edit()

    def _current_interface_item(self):
        items = self._tree.selectedItems()
        if not items:
            return None
        item = items[0]
        while item is not None and item.data(0, ROLE_KIND) != "interface":
            item = item.parent()
        return item

    def _update_buttons(self):
        has_interface = self._current_interface_item() is not None
        self._edit_btn.setEnabled(has_interface)
        self._delete_btn.setEnabled(has_interface)

    def _project_dir(self):
        if self.main_editor is not None:
            try:
                return self.main_editor.project.get_directory()
            except Exception:
                pass
        return os.getcwd()

    def _resolve_path(self, path):
        if not path:
            return ""
        if path.startswith("res://"):
            return str(Path(self._project_dir()) / path[len("res://"):])
        return path

    @staticmethod
    def _load_json(raw):
        if not raw:
            return []
        try:
            data = json.loads(raw)
            return data if isinstance(data, list) else []
        except (ValueError, TypeError):
            return []

    @staticmethod
    def _load_obj(raw):
        if not raw:
            return {}
        try:
            data = json.loads(raw)
            return data if isinstance(data, dict) else {}
        except (ValueError, TypeError):
            return {}
