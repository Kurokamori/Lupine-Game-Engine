"""
Node Signals Panel

Dock that lists the selected node's signals (aggregated from the
node itself and each attached component) and their connections, and lets the
user wire signals to handler methods on other nodes via a connect dialog.
Connections are stored on the engine objects and persisted in the scene file.
"""

import json

from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QTreeWidget, QTreeWidgetItem,
    QPushButton, QLabel
)
from PyQt6.QtCore import Qt

from panels.base_panel import EditorPanel
from dialogs.connect_signal_dialog import ConnectSignalDialog

# Item roles
ROLE_KIND = Qt.ItemDataRole.UserRole          # "signal" | "connection"
ROLE_DATA = Qt.ItemDataRole.UserRole + 1       # dict payload


class NodeSignalsPanel(EditorPanel):
    """Shows and edits signal connections for the selected node."""

    def __init__(self, parent=None):
        self.editor_bridge = None
        self._current_node = None
        super().__init__("Node Signals", parent)

    def _setup_panel(self):
        container = QWidget()
        layout = QVBoxLayout(container)
        layout.setContentsMargins(6, 6, 6, 6)

        self._info_label = QLabel("No node selected.")
        self._info_label.setWordWrap(True)
        layout.addWidget(self._info_label)

        self._tree = QTreeWidget()
        self._tree.setHeaderLabels(["Signal / Connection"])
        self._tree.setColumnCount(1)
        self._tree.itemDoubleClicked.connect(self._on_item_double_clicked)
        self._tree.itemSelectionChanged.connect(self._update_buttons)
        layout.addWidget(self._tree, 1)

        button_row = QHBoxLayout()
        self._connect_btn = QPushButton("Connect…")
        self._connect_btn.clicked.connect(self._on_connect)
        self._connect_btn.setEnabled(False)
        self._disconnect_btn = QPushButton("Disconnect")
        self._disconnect_btn.clicked.connect(self._on_disconnect)
        self._disconnect_btn.setEnabled(False)
        self._refresh_btn = QPushButton("Refresh")
        self._refresh_btn.clicked.connect(self.refresh)
        button_row.addWidget(self._connect_btn)
        button_row.addWidget(self._disconnect_btn)
        button_row.addStretch()
        button_row.addWidget(self._refresh_btn)
        layout.addLayout(button_row)

        self.content_layout.addWidget(container)

    def set_node(self, node):
        """Called when the editor selection changes."""
        self._current_node = node
        self.refresh()

    def refresh(self):
        self._tree.clear()
        if not self.editor_bridge or not self._current_node:
            self._info_label.setText("No node selected.")
            self._update_buttons()
            return

        try:
            node_name = self._current_node.get_name()
        except Exception:
            node_name = "<node>"
        self._info_label.setText(f"Signals for <b>{node_name}</b>")

        signals = self._load_json(self.editor_bridge.get_node_signals(self._current_node))
        connections = self._load_json(self.editor_bridge.get_node_connections(self._current_node))

        # Group connections by (source, signal) for nesting under the right item.
        conn_map = {}
        for conn in connections:
            key = (conn.get("source", "node"), conn.get("signal", ""))
            conn_map.setdefault(key, []).append(conn)

        for sig in signals:
            source = sig.get("source", "node")
            source_label = sig.get("source_label", "Node")
            signal_name = sig.get("signal", "")
            args = sig.get("args", [])
            arg_text = ", ".join(a.get("name", "") for a in args)
            label = f"{signal_name}({arg_text})  ·  {source_label}"

            sig_item = QTreeWidgetItem([label])
            sig_item.setData(0, ROLE_KIND, "signal")
            sig_item.setData(0, ROLE_DATA, sig)
            self._tree.addTopLevelItem(sig_item)

            for conn in conn_map.get((source, signal_name), []):
                target = conn.get("target_path") or conn.get("target_uuid", "?")
                flags = int(conn.get("flags", 0))
                flag_text = self._flags_text(flags)
                conn_label = f"→ {target} :: {conn.get('method', '')}{flag_text}"
                conn_item = QTreeWidgetItem([conn_label])
                conn_item.setData(0, ROLE_KIND, "connection")
                conn_item.setData(0, ROLE_DATA, conn)
                sig_item.addChild(conn_item)

        self._tree.expandAll()
        self._update_buttons()

    @staticmethod
    def _flags_text(flags: int) -> str:
        parts = []
        if flags & 1:
            parts.append("deferred")
        if flags & 2:
            parts.append("one-shot")
        return f"  [{', '.join(parts)}]" if parts else ""

    @staticmethod
    def _load_json(raw):
        if not raw:
            return []
        try:
            data = json.loads(raw)
            return data if isinstance(data, list) else []
        except (ValueError, TypeError):
            return []

    def _update_buttons(self):
        item = self._current_item()
        kind = item.data(0, ROLE_KIND) if item else None
        self._connect_btn.setEnabled(kind == "signal")
        self._disconnect_btn.setEnabled(kind == "connection")

    def _current_item(self):
        items = self._tree.selectedItems()
        return items[0] if items else None

    def _on_item_double_clicked(self, item, _column):
        if item and item.data(0, ROLE_KIND) == "signal":
            self._on_connect()

    def _on_connect(self):
        item = self._current_item()
        if not item or item.data(0, ROLE_KIND) != "signal":
            return
        sig = item.data(0, ROLE_DATA)
        root = self.editor_bridge.get_root_node()
        if not root:
            return

        dialog = ConnectSignalDialog(
            sig.get("signal", ""), sig.get("args", []),
            sig.get("source_label", "Node"), root, self)
        if dialog.exec():
            target_node, method, flags = dialog.get_result()
            if target_node and method:
                source = sig.get("source", "node")
                source_component_uuid = "" if source == "node" else source
                self.editor_bridge.add_connection(
                    self._current_node, source_component_uuid,
                    sig.get("signal", ""), target_node, method, flags)
                self.refresh()

    def _on_disconnect(self):
        item = self._current_item()
        if not item or item.data(0, ROLE_KIND) != "connection":
            return
        conn = item.data(0, ROLE_DATA)
        target_node = self._resolve_node_by_uuid(conn.get("target_uuid", ""))
        if not target_node:
            return
        source = conn.get("source", "node")
        source_component_uuid = "" if source == "node" else source
        self.editor_bridge.remove_connection(
            self._current_node, source_component_uuid,
            conn.get("signal", ""), target_node, conn.get("method", ""))
        self.refresh()

    def _resolve_node_by_uuid(self, uuid_str: str):
        if not uuid_str or not self.editor_bridge:
            return None
        root = self.editor_bridge.get_root_node()
        return self._find_by_uuid(root, uuid_str)

    def _find_by_uuid(self, node, uuid_str: str):
        if not node:
            return None
        try:
            if str(node.get_uuid()) == uuid_str:
                return node
            for child in node.get_children():
                found = self._find_by_uuid(child, uuid_str)
                if found:
                    return found
        except Exception:
            return None
        return None
