"""
Connect Signal Dialog

Godot-style dialog for connecting a node/component signal to a handler method on
a target node. The target node's attached script(s) provide the handler; the
connection is persisted in the scene file by the engine.
"""

from PyQt6.QtWidgets import (
    QDialog, QVBoxLayout, QHBoxLayout, QLabel, QTreeWidget, QTreeWidgetItem,
    QLineEdit, QCheckBox, QPushButton, QDialogButtonBox, QGroupBox
)
from PyQt6.QtCore import Qt

# Connection flag bits (mirror core::ConnectFlags / LC_CONNECT_*).
CONNECT_DEFERRED = 1
CONNECT_ONESHOT = 2


class ConnectSignalDialog(QDialog):
    """Collects a target node, handler method name, and connection flags."""

    def __init__(self, signal_name: str, signal_args, source_label: str,
                 root_node, parent=None):
        super().__init__(parent)
        self._signal_name = signal_name
        self._signal_args = signal_args or []
        self._root_node = root_node
        self._selected_node = None

        self.setWindowTitle(f"Connect Signal: {signal_name}")
        self.setMinimumSize(420, 480)
        self._setup_ui(source_label)
        self._populate_tree()

    def _setup_ui(self, source_label: str):
        layout = QVBoxLayout(self)

        arg_names = ", ".join(
            f"{a.get('type', 'var')} {a.get('name', '')}".strip()
            for a in self._signal_args
        )
        header = QLabel(f"<b>{source_label}</b> · signal "
                        f"<code>{self._signal_name}({arg_names})</code>")
        header.setTextFormat(Qt.TextFormat.RichText)
        header.setWordWrap(True)
        layout.addWidget(header)

        layout.addWidget(QLabel("Connect to node:"))
        self._tree = QTreeWidget()
        self._tree.setHeaderLabels(["Node", "Type"])
        self._tree.itemSelectionChanged.connect(self._on_selection_changed)
        layout.addWidget(self._tree, 1)

        method_box = QGroupBox("Handler")
        method_layout = QVBoxLayout(method_box)
        method_layout.addWidget(QLabel("Method name on target node's script:"))
        self._method_edit = QLineEdit()
        self._method_edit.setText(self._default_method_name())
        method_layout.addWidget(self._method_edit)
        layout.addWidget(method_box)

        flags_layout = QHBoxLayout()
        self._deferred_check = QCheckBox("Deferred")
        self._deferred_check.setToolTip("Invoke the handler at the end of the frame")
        self._oneshot_check = QCheckBox("One-shot")
        self._oneshot_check.setToolTip("Disconnect automatically after the first call")
        flags_layout.addWidget(self._deferred_check)
        flags_layout.addWidget(self._oneshot_check)
        flags_layout.addStretch()
        layout.addLayout(flags_layout)

        self._buttons = QDialogButtonBox(
            QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel)
        self._buttons.accepted.connect(self.accept)
        self._buttons.rejected.connect(self.reject)
        self._buttons.button(QDialogButtonBox.StandardButton.Ok).setEnabled(False)
        layout.addWidget(self._buttons)

    def _default_method_name(self) -> str:
        return f"_on_{self._signal_name}"

    def _populate_tree(self):
        self._tree.clear()
        if not self._root_node:
            return
        root_item = self._build_item(self._root_node)
        self._tree.addTopLevelItem(root_item)
        self._tree.expandAll()

    def _build_item(self, node) -> QTreeWidgetItem:
        try:
            name = node.get_name()
            type_name = node.get_type_name()
        except Exception:
            name, type_name = "<node>", ""
        item = QTreeWidgetItem([name, type_name])
        item.setData(0, Qt.ItemDataRole.UserRole, node)
        try:
            for child in node.get_children():
                item.addChild(self._build_item(child))
        except Exception:
            pass
        return item

    def _on_selection_changed(self):
        items = self._tree.selectedItems()
        if items:
            self._selected_node = items[0].data(0, Qt.ItemDataRole.UserRole)
        else:
            self._selected_node = None
        ok_button = self._buttons.button(QDialogButtonBox.StandardButton.Ok)
        ok_button.setEnabled(self._selected_node is not None)

    def get_result(self):
        """Return (target_node, method_name, flags) after the dialog is accepted."""
        flags = 0
        if self._deferred_check.isChecked():
            flags |= CONNECT_DEFERRED
        if self._oneshot_check.isChecked():
            flags |= CONNECT_ONESHOT
        return self._selected_node, self._method_edit.text().strip(), flags
