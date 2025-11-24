"""
Add Node/Prefab Dialog
Godot-style combined dialog for adding nodes or instantiating prefabs
"""

from PyQt6.QtWidgets import (QDialog, QVBoxLayout, QHBoxLayout, QLineEdit,
                             QTreeWidget, QTreeWidgetItem, QPushButton,
                             QLabel, QTabWidget, QWidget, QStyle)
from PyQt6.QtCore import Qt, pyqtSignal
from PyQt6.QtGui import QIcon, QFont, QColor, QPalette, QPixmap, QPainter
import lupine_engine as le
from editor.theme import get_theme_manager
import re


class AddNodePrefabDialog(QDialog):
    """Combined dialog for adding nodes or instantiating prefabs"""

    # Signals
    node_type_selected = pyqtSignal(str)  # Emits the node type name
    prefab_selected = pyqtSignal(str)  # Emits the prefab path

    def __init__(self, editor_bridge, parent=None):
        super().__init__(parent)
        self.editor_bridge = editor_bridge
        self.selected_type = None
        self.selected_path = None
        self.selection_mode = None  # "node" or "prefab"

        self.setWindowTitle("Add Node/Prefab")
        self.setMinimumSize(600, 650)
        self.setModal(True)

        self._setup_ui()
        self._load_types()

    def _setup_ui(self):
        """Setup the dialog UI"""
        layout = QVBoxLayout()

        # Search bar at the top
        search_layout = QHBoxLayout()
        search_label = QLabel("Search:")
        self.search_input = QLineEdit()
        self.search_input.setPlaceholderText("Type to search...")
        self.search_input.textChanged.connect(self._on_search_changed)
        search_layout.addWidget(search_label)
        search_layout.addWidget(self.search_input)
        layout.addLayout(search_layout)

        # Tab widget for nodes and prefabs
        self.tab_widget = QTabWidget()

        # Store theme for later use
        theme = get_theme_manager().get_current_theme()
        self.colors = theme.colors

        # Create arrow icons for categories
        self._create_arrow_icons()

        # Nodes tab
        self.nodes_tree = QTreeWidget()
        self.nodes_tree.setHeaderLabel("Node Types")
        self.nodes_tree.itemDoubleClicked.connect(lambda item, col: self._on_item_double_clicked(item, col, "node"))
        self.nodes_tree.itemClicked.connect(lambda item, col: self._on_item_clicked(item, col, "node"))
        self.nodes_tree.itemExpanded.connect(lambda item: self._on_item_expanded(item, "node"))
        self.nodes_tree.itemCollapsed.connect(lambda item: self._on_item_collapsed(item, "node"))

        # Hide default branch indicators for nodes tree
        self.nodes_tree.setStyleSheet(f"""
            QTreeView::branch {{
                background: transparent;
            }}
            QTreeView::branch:has-siblings:!adjoins-item {{
                background: transparent;
            }}
            QTreeView::branch:has-siblings:adjoins-item {{
                background: transparent;
            }}
            QTreeView::branch:!has-children:!has-siblings:adjoins-item {{
                background: transparent;
            }}
            QTreeView::branch:has-children:!has-siblings:closed,
            QTreeView::branch:closed:has-children:has-siblings {{
                background: transparent;
                border: none;
                image: none;
            }}
            QTreeView::branch:open:has-children:!has-siblings,
            QTreeView::branch:open:has-children:has-siblings {{
                background: transparent;
                border: none;
                image: none;
            }}
        """)
        self.tab_widget.addTab(self.nodes_tree, "Nodes")

        # Prefabs tab
        self.prefabs_tree = QTreeWidget()
        self.prefabs_tree.setHeaderLabel("Prefabs")
        self.prefabs_tree.itemDoubleClicked.connect(lambda item, col: self._on_item_double_clicked(item, col, "prefab"))
        self.prefabs_tree.itemClicked.connect(lambda item, col: self._on_item_clicked(item, col, "prefab"))
        self.prefabs_tree.itemExpanded.connect(lambda item: self._on_item_expanded(item, "prefab"))
        self.prefabs_tree.itemCollapsed.connect(lambda item: self._on_item_collapsed(item, "prefab"))

        # Hide default branch indicators for prefabs tree
        self.prefabs_tree.setStyleSheet(f"""
            QTreeView::branch {{
                background: transparent;
            }}
            QTreeView::branch:has-siblings:!adjoins-item {{
                background: transparent;
            }}
            QTreeView::branch:has-siblings:adjoins-item {{
                background: transparent;
            }}
            QTreeView::branch:!has-children:!has-siblings:adjoins-item {{
                background: transparent;
            }}
            QTreeView::branch:has-children:!has-siblings:closed,
            QTreeView::branch:closed:has-children:has-siblings {{
                background: transparent;
                border: none;
                image: none;
            }}
            QTreeView::branch:open:has-children:!has-siblings,
            QTreeView::branch:open:has-children:has-siblings {{
                background: transparent;
                border: none;
                image: none;
            }}
        """)
        self.tab_widget.addTab(self.prefabs_tree, "Prefabs")

        layout.addWidget(self.tab_widget)

        # Buttons at the bottom
        button_layout = QHBoxLayout()
        button_layout.addStretch()

        self.ok_button = QPushButton("Add")
        self.ok_button.setEnabled(False)
        self.ok_button.clicked.connect(self.accept)
        button_layout.addWidget(self.ok_button)

        cancel_button = QPushButton("Cancel")
        cancel_button.clicked.connect(self.reject)
        button_layout.addWidget(cancel_button)

        layout.addLayout(button_layout)

        self.setLayout(layout)

    def _create_arrow_icons(self):
        """Create custom colored arrow icons for category items"""
        from PyQt6.QtGui import QPolygon
        from PyQt6.QtCore import QPoint

        size = 14

        # Create closed arrow (right-pointing triangle)
        closed_pixmap = QPixmap(size, size)
        closed_pixmap.fill(Qt.GlobalColor.transparent)
        painter = QPainter(closed_pixmap)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)

        color = QColor(self.colors.accent_color)
        painter.setPen(Qt.PenStyle.NoPen)
        painter.setBrush(color)

        # Right-pointing triangle
        triangle = QPolygon([
            QPoint(4, 3),
            QPoint(4, 11),
            QPoint(10, 7)
        ])
        painter.drawPolygon(triangle)
        painter.end()

        # Create open arrow (down-pointing triangle)
        open_pixmap = QPixmap(size, size)
        open_pixmap.fill(Qt.GlobalColor.transparent)
        painter = QPainter(open_pixmap)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)

        painter.setPen(Qt.PenStyle.NoPen)
        painter.setBrush(color)

        # Down-pointing triangle
        triangle = QPolygon([
            QPoint(3, 4),
            QPoint(11, 4),
            QPoint(7, 10)
        ])
        painter.drawPolygon(triangle)
        painter.end()

        # Store icons
        self.closed_icon = QIcon(closed_pixmap)
        self.open_icon = QIcon(open_pixmap)

    def _on_item_expanded(self, item, mode):
        """Update icon when item is expanded"""
        if mode == "node":
            if item.data(0, Qt.ItemDataRole.UserRole) is None:  # It's a category
                item.setIcon(0, self.open_icon)
        else:  # prefab
            type_name = item.data(1, Qt.ItemDataRole.UserRole)
            if type_name is None:  # It's a category
                item.setIcon(0, self.open_icon)

    def _on_item_collapsed(self, item, mode):
        """Update icon when item is collapsed"""
        if mode == "node":
            if item.data(0, Qt.ItemDataRole.UserRole) is None:  # It's a category
                item.setIcon(0, self.closed_icon)
        else:  # prefab
            type_name = item.data(1, Qt.ItemDataRole.UserRole)
            if type_name is None:  # It's a category
                item.setIcon(0, self.closed_icon)

    def _format_display_name(self, name):
        """
        Convert CamelCase names to spaced format.
        e.g., 'CollisionMesh3D' -> 'Collision Mesh 3D'
        """
        # Insert space before uppercase letters and numbers
        spaced = re.sub(r'([a-z])([A-Z0-9])', r'\1 \2', name)
        # Insert space before uppercase letter followed by lowercase
        spaced = re.sub(r'([A-Z]+)([A-Z][a-z])', r'\1 \2', spaced)
        # Insert space before standalone numbers
        spaced = re.sub(r'([a-zA-Z])(\d)', r'\1 \2', spaced)
        return spaced

    def _build_nested_tree(self, items_with_categories, tree_widget, mode):
        """
        Build a nested tree structure from items with slash-separated category paths.

        Args:
            items_with_categories: List of tuples (category_path, item_data)
            tree_widget: The tree widget to populate
            mode: "node" or "prefab"

        Returns:
            Dict mapping category paths to QTreeWidgetItem objects
        """
        # Dictionary to store all category nodes: path -> QTreeWidgetItem
        category_nodes = {}

        # First, create all category nodes
        all_category_paths = set()
        for category_path, _ in items_with_categories:
            parts = category_path.split('/')
            # Add all intermediate paths
            for i in range(1, len(parts) + 1):
                path = '/'.join(parts[:i])
                all_category_paths.add(path)

        # Sort paths by depth (shortest first) to ensure parents exist before children
        sorted_paths = sorted(all_category_paths, key=lambda p: p.count('/'))

        # Create category nodes
        for path in sorted_paths:
            parts = path.split('/')
            category_name = parts[-1]
            depth = len(parts)

            if depth == 1:
                # Top-level category - secondary accent background color, +4pt font, start collapsed
                category_item = QTreeWidgetItem(tree_widget)
                category_item.setText(0, category_name)
                category_item.setData(0, Qt.ItemDataRole.UserRole, None)
                if mode == "prefab":
                    category_item.setData(1, Qt.ItemDataRole.UserRole, None)
                category_item.setExpanded(False)  # Start collapsed
                category_item.setIcon(0, self.closed_icon)  # Set arrow icon

                # Apply styling: secondary accent background color and larger font
                font = category_item.font(0)
                font.setPointSize(font.pointSize() + 4)
                font.setBold(True)
                category_item.setFont(0, font)
                category_item.setBackground(0, QColor(self.colors.secondary_accent_color))
                category_item.setForeground(0, QColor(self.colors.text_primary))

                category_nodes[path] = category_item
            else:
                # Nested category - tertiary background color, +2pt font, start collapsed
                parent_path = '/'.join(parts[:-1])
                parent_item = category_nodes.get(parent_path)
                if parent_item:
                    category_item = QTreeWidgetItem(parent_item)
                    category_item.setText(0, category_name)
                    category_item.setData(0, Qt.ItemDataRole.UserRole, None)
                    if mode == "prefab":
                        category_item.setData(1, Qt.ItemDataRole.UserRole, None)
                    category_item.setExpanded(False)  # Start collapsed
                    category_item.setIcon(0, self.closed_icon)  # Set arrow icon

                    # Apply styling: tertiary background color and medium font
                    font = category_item.font(0)
                    font.setPointSize(font.pointSize() + 2)
                    font.setBold(True)
                    category_item.setFont(0, font)
                    category_item.setBackground(0, QColor(self.colors.tertiary_color))
                    category_item.setForeground(0, QColor(self.colors.text_primary))

                    category_nodes[path] = category_item

        return category_nodes

    def _load_types(self):
        """Load all types from the editor bridge"""
        self._load_node_types()
        self._load_prefab_types()

    def _load_node_types(self):
        """Load node types and populate the nodes tree"""
        self.nodes_tree.clear()

        # Get all node types
        node_types = self.editor_bridge.get_node_types()

        # Organize by subcategory path
        items_by_category = {}
        for node_type in node_types:
            category_path = node_type.subcategory if node_type.subcategory else "Other"
            if category_path not in items_by_category:
                items_by_category[category_path] = []
            items_by_category[category_path].append(node_type)

        # Build nested tree structure
        items_with_categories = [(path, items) for path, items in items_by_category.items()]
        category_nodes = self._build_nested_tree(items_with_categories, self.nodes_tree, "node")

        # Add node types under their respective categories
        for category_path, nodes in items_by_category.items():
            parent_item = category_nodes.get(category_path)
            if parent_item:
                for node_type in sorted(nodes, key=lambda t: t.type_name):
                    type_item = QTreeWidgetItem(parent_item)
                    display_name = self._format_display_name(node_type.type_name)
                    if node_type.is_built_in:
                        display_name += " (Built-in)"
                    type_item.setText(0, display_name)
                    type_item.setData(0, Qt.ItemDataRole.UserRole, node_type.type_name)
                    type_item.setToolTip(0, f"Type: {node_type.type_name}\nPath: {node_type.file_path if node_type.file_path else 'Built-in'}")

                    # Increase font size for node items
                    item_font = type_item.font(0)
                    item_font.setPointSize(item_font.pointSize() + 1)
                    type_item.setFont(0, item_font)

    def _load_prefab_types(self):
        """Load prefab types and populate the prefabs tree"""
        self.prefabs_tree.clear()

        # Get all prefab types
        prefab_types = self.editor_bridge.get_prefab_types()

        # Organize by subcategory path
        items_by_category = {}
        for prefab_type in prefab_types:
            category_path = prefab_type.subcategory if prefab_type.subcategory else "Other"
            if category_path not in items_by_category:
                items_by_category[category_path] = []
            items_by_category[category_path].append(prefab_type)

        # Build nested tree structure
        items_with_categories = [(path, items) for path, items in items_by_category.items()]
        category_nodes = self._build_nested_tree(items_with_categories, self.prefabs_tree, "prefab")

        # Add prefab types under their respective categories
        for category_path, prefabs in items_by_category.items():
            parent_item = category_nodes.get(category_path)
            if parent_item:
                for prefab_type in sorted(prefabs, key=lambda t: t.type_name):
                    type_item = QTreeWidgetItem(parent_item)
                    display_name = self._format_display_name(prefab_type.type_name)
                    if prefab_type.is_built_in:
                        display_name += " (Built-in)"
                    type_item.setText(0, display_name)
                    type_item.setData(0, Qt.ItemDataRole.UserRole, prefab_type.file_path)
                    type_item.setData(1, Qt.ItemDataRole.UserRole, prefab_type.type_name)
                    type_item.setToolTip(0, f"Name: {prefab_type.type_name}\nPath: {prefab_type.file_path if prefab_type.file_path else 'Built-in'}")

                    # Increase font size for prefab items
                    item_font = type_item.font(0)
                    item_font.setPointSize(item_font.pointSize() + 1)
                    type_item.setFont(0, item_font)

    def _on_search_changed(self, text):
        """Filter both trees based on search text"""
        search_text = text.lower()

        # Filter nodes tree
        self._filter_tree(self.nodes_tree, search_text, "node")
        # Filter prefabs tree
        self._filter_tree(self.prefabs_tree, search_text, "prefab")

    def _filter_tree(self, tree_widget, search_text, mode):
        """Filter a tree widget based on search text"""
        if not search_text:
            self._show_all_items(tree_widget)
            return

        # Filter tree recursively
        root = tree_widget.invisibleRootItem()
        for i in range(root.childCount()):
            self._filter_tree_recursive(root.child(i), search_text, mode)

    def _filter_tree_recursive(self, item, search_text, mode):
        """
        Recursively filter tree items based on search text.

        Returns True if this item or any of its children match the search.
        """
        # Check if this is a type item (has type data)
        if mode == "node":
            type_name = item.data(0, Qt.ItemDataRole.UserRole)
        else:  # prefab
            type_name = item.data(1, Qt.ItemDataRole.UserRole)

        is_match = False

        if type_name:
            # This is a type item
            is_match = search_text in type_name.lower()
        else:
            # This is a category - check all children
            for i in range(item.childCount()):
                child = item.child(i)
                if self._filter_tree_recursive(child, search_text, mode):
                    is_match = True

        # Show/hide this item based on match
        item.setHidden(not is_match)
        if is_match and not type_name:
            # If this is a matching category, expand it
            item.setExpanded(True)

        return is_match

    def _show_all_items(self, tree_widget):
        """Show all items in a tree"""
        root = tree_widget.invisibleRootItem()
        for i in range(root.childCount()):
            self._show_all_items_recursive(root.child(i))

    def _show_all_items_recursive(self, item):
        """Recursively show all items in the tree"""
        item.setHidden(False)
        for i in range(item.childCount()):
            self._show_all_items_recursive(item.child(i))

    def _on_item_clicked(self, item, column, mode):
        """Handle item click"""
        if mode == "node":
            type_name = item.data(0, Qt.ItemDataRole.UserRole)
            if type_name:
                self.selected_type = type_name
                self.selected_path = None
                self.selection_mode = "node"
                self.ok_button.setEnabled(True)
            else:
                # It's a category - toggle expansion
                self._clear_selection()
                item.setExpanded(not item.isExpanded())
        else:  # prefab
            file_path = item.data(0, Qt.ItemDataRole.UserRole)
            if file_path:
                self.selected_path = file_path
                self.selected_type = None
                self.selection_mode = "prefab"
                self.ok_button.setEnabled(True)
            else:
                # It's a category - toggle expansion
                self._clear_selection()
                item.setExpanded(not item.isExpanded())

    def _on_item_double_clicked(self, item, column, mode):
        """Handle item double-click"""
        if mode == "node":
            type_name = item.data(0, Qt.ItemDataRole.UserRole)
            if type_name:
                self.selected_type = type_name
                self.selected_path = None
                self.selection_mode = "node"
                self.accept()
        else:  # prefab
            file_path = item.data(0, Qt.ItemDataRole.UserRole)
            if file_path:
                self.selected_path = file_path
                self.selected_type = None
                self.selection_mode = "prefab"
                self.accept()

    def _clear_selection(self):
        """Clear the current selection"""
        self.selected_type = None
        self.selected_path = None
        self.selection_mode = None
        self.ok_button.setEnabled(False)

    def get_selection(self):
        """Get the selection (mode, value)"""
        if self.selection_mode == "node":
            return ("node", self.selected_type)
        elif self.selection_mode == "prefab":
            return ("prefab", self.selected_path)
        return (None, None)

    def reload_types(self):
        """Reload all types from the editor bridge"""
        self._load_types()
