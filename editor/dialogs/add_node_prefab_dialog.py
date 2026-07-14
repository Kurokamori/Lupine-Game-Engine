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

    # Item data roles (uniform across every tree in the dialog)
    KIND_ROLE = Qt.ItemDataRole.UserRole          # None for categories, else "node"/"prefab"/"component_default"
    VALUE_ROLE = int(Qt.ItemDataRole.UserRole) + 1  # node type name / prefab file path / component type name
    BASE_NODE_ROLE = int(Qt.ItemDataRole.UserRole) + 2  # base node type for component_default items

    # Category that groups the auto-generated per-component prefabs
    COMPONENT_DEFAULT_CATEGORY = "Component Default"

    def __init__(self, editor_bridge, parent=None):
        super().__init__(parent)
        self.editor_bridge = editor_bridge

        # Current selection
        self.selection_kind = None      # "node" | "prefab" | "component_default"
        self.selected_value = None      # node type / prefab path / component type
        self.selected_base_node = None  # base node type (component_default only)

        self.setWindowTitle("Add Node/Prefab")
        self.setMinimumSize(600, 650)
        self.setModal(True)

        self._setup_ui()
        self._load_types()

        # Open fully expanded with the search box focused so typing searches immediately
        for tree in (self.all_tree, self.nodes_tree, self.prefabs_tree):
            self._expand_all(tree)
        self.search_input.setFocus()

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

        # Tab widget for All / Nodes / Prefabs
        self.tab_widget = QTabWidget()

        # Store theme for later use
        theme = get_theme_manager().get_current_theme()
        self.colors = theme.colors

        # Create arrow icons for categories
        self._create_arrow_icons()

        # All tab (default) - shows both nodes and prefabs
        self.all_tree = self._create_tree("Nodes & Prefabs")
        self.tab_widget.addTab(self.all_tree, "All")

        # Nodes tab
        self.nodes_tree = self._create_tree("Node Types")
        self.tab_widget.addTab(self.nodes_tree, "Nodes")

        # Prefabs tab
        self.prefabs_tree = self._create_tree("Prefabs")
        self.tab_widget.addTab(self.prefabs_tree, "Prefabs")

        # All tab is the default
        self.tab_widget.setCurrentIndex(0)

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

    def _create_tree(self, header_label):
        """Create and configure a tree widget with the shared styling and handlers"""
        tree = QTreeWidget()
        tree.setHeaderLabel(header_label)
        tree.itemDoubleClicked.connect(self._on_item_double_clicked)
        tree.itemClicked.connect(self._on_item_clicked)
        tree.itemExpanded.connect(self._on_item_expanded)
        tree.itemCollapsed.connect(self._on_item_collapsed)

        # Hide default branch indicators (we draw our own arrow icons)
        tree.setStyleSheet(f"""
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
        return tree

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

    def _is_category(self, item):
        """True if the item is a category (no selectable kind)"""
        return item.data(0, self.KIND_ROLE) is None

    def _on_item_expanded(self, item):
        """Update icon when item is expanded"""
        if self._is_category(item):
            item.setIcon(0, self.open_icon)

    def _on_item_collapsed(self, item):
        """Update icon when item is collapsed"""
        if self._is_category(item):
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

    def _base_node_for_component(self, component_type):
        """
        Pick the base node type (Node2D / Node3D) for a component default prefab
        based on the component's spatial type, inferred from its subcategory and
        type name (matching how the engine classifies components).
        """
        type_name = component_type.type_name or ""
        subcategory = component_type.subcategory or ""
        segments = subcategory.split('/')

        if any(seg == "3D" or seg.endswith("3D") for seg in segments) or type_name.endswith("3D"):
            return "Node3D"
        if any(seg == "2D" or seg.endswith("2D") for seg in segments) or type_name.endswith("2D"):
            return "Node2D"
        # UI / Canvas and everything else default to the 2D base node
        return "Node2D"

    def _build_nested_tree(self, category_paths, tree_widget):
        """
        Build a nested tree structure from slash-separated category paths.

        Args:
            category_paths: Iterable of category path strings like "Physics/2D"
            tree_widget: The tree widget to populate

        Returns:
            Dict mapping category paths to QTreeWidgetItem objects
        """
        # Dictionary to store all category nodes: path -> QTreeWidgetItem
        category_nodes = {}

        # First, gather all intermediate category paths
        all_category_paths = set()
        for category_path in category_paths:
            parts = category_path.split('/')
            for i in range(1, len(parts) + 1):
                all_category_paths.add('/'.join(parts[:i]))

        # Sort paths by depth (shortest first) so parents exist before children
        sorted_paths = sorted(all_category_paths, key=lambda p: p.count('/'))

        # Create category nodes
        for path in sorted_paths:
            parts = path.split('/')
            category_name = parts[-1]
            depth = len(parts)

            if depth == 1:
                # Top-level category - secondary accent background, +4pt font
                category_item = QTreeWidgetItem(tree_widget)
                category_item.setText(0, category_name)
                category_item.setData(0, self.KIND_ROLE, None)
                category_item.setExpanded(False)
                category_item.setIcon(0, self.closed_icon)

                font = category_item.font(0)
                font.setPointSize(font.pointSize() + 4)
                font.setBold(True)
                category_item.setFont(0, font)
                category_item.setBackground(0, QColor(self.colors.secondary_accent_color))
                category_item.setForeground(0, QColor(self.colors.text_primary))

                category_nodes[path] = category_item
            else:
                # Nested category - tertiary background, +2pt font
                parent_path = '/'.join(parts[:-1])
                parent_item = category_nodes.get(parent_path)
                if parent_item:
                    category_item = QTreeWidgetItem(parent_item)
                    category_item.setText(0, category_name)
                    category_item.setData(0, self.KIND_ROLE, None)
                    category_item.setExpanded(False)
                    category_item.setIcon(0, self.closed_icon)

                    font = category_item.font(0)
                    font.setPointSize(font.pointSize() + 2)
                    font.setBold(True)
                    category_item.setFont(0, font)
                    category_item.setBackground(0, QColor(self.colors.tertiary_color))
                    category_item.setForeground(0, QColor(self.colors.text_primary))

                    category_nodes[path] = category_item

        return category_nodes

    def _add_leaf_item(self, parent_item, display_name, kind, value, tooltip, base_node=None):
        """Create a selectable leaf item under a category"""
        item = QTreeWidgetItem(parent_item)
        item.setText(0, display_name)
        item.setData(0, self.KIND_ROLE, kind)
        item.setData(0, self.VALUE_ROLE, value)
        if base_node is not None:
            item.setData(0, self.BASE_NODE_ROLE, base_node)
        item.setToolTip(0, tooltip)

        item_font = item.font(0)
        item_font.setPointSize(item_font.pointSize() + 1)
        item.setFont(0, item_font)
        return item

    def _group_by_category(self, types, prefix=""):
        """
        Group type infos by their subcategory path, optionally prefixed.

        Returns a dict mapping full category path -> list of type infos.
        """
        items_by_category = {}
        for type_info in types:
            category_path = type_info.subcategory if type_info.subcategory else "Other"
            if prefix:
                category_path = f"{prefix}/{category_path}"
            items_by_category.setdefault(category_path, []).append(type_info)
        return items_by_category

    def _load_types(self):
        """Load all types into the three trees"""
        node_types = self.editor_bridge.get_node_types()
        prefab_types = self.editor_bridge.get_prefab_types()
        component_types = self.editor_bridge.get_component_types()

        # Nodes tab
        self.nodes_tree.clear()
        self._populate_nodes(self.nodes_tree, node_types, prefix="")

        # Prefabs tab (real prefabs + per-component default prefabs)
        self.prefabs_tree.clear()
        self._populate_prefabs(self.prefabs_tree, prefab_types, component_types, prefix="")

        # All tab (nodes grouped under "Nodes", prefabs under "Prefabs")
        self.all_tree.clear()
        self._populate_nodes(self.all_tree, node_types, prefix="Nodes")
        self._populate_prefabs(self.all_tree, prefab_types, component_types, prefix="Prefabs")

    def _populate_nodes(self, tree_widget, node_types, prefix=""):
        """Populate node type leaves into a tree under an optional prefix category"""
        items_by_category = self._group_by_category(node_types, prefix)
        category_nodes = self._build_nested_tree(items_by_category.keys(), tree_widget)

        for category_path, nodes in items_by_category.items():
            parent_item = category_nodes.get(category_path)
            if not parent_item:
                continue
            for node_type in sorted(nodes, key=lambda t: t.type_name):
                display_name = self._format_display_name(node_type.type_name)
                if node_type.is_built_in:
                    display_name += " (Built-in)"
                tooltip = f"Type: {node_type.type_name}\nPath: {node_type.file_path if node_type.file_path else 'Built-in'}"
                self._add_leaf_item(parent_item, display_name, "node", node_type.type_name, tooltip)

    def _populate_prefabs(self, tree_widget, prefab_types, component_types, prefix=""):
        """Populate real prefab leaves plus per-component default prefab leaves"""
        # Real prefabs grouped by their own subcategory
        items_by_category = self._group_by_category(prefab_types, prefix)

        # Per-component default prefabs grouped under "Component Default/<component subcategory>"
        comp_default_root = self.COMPONENT_DEFAULT_CATEGORY
        if prefix:
            comp_default_root = f"{prefix}/{comp_default_root}"
        component_defaults_by_category = {}
        for component_type in component_types:
            sub = component_type.subcategory if component_type.subcategory else "Other"
            category_path = f"{comp_default_root}/{sub}"
            component_defaults_by_category.setdefault(category_path, []).append(component_type)

        # Build the full category tree from both sets at once
        all_category_paths = list(items_by_category.keys()) + list(component_defaults_by_category.keys())
        category_nodes = self._build_nested_tree(all_category_paths, tree_widget)

        # Add real prefab leaves
        for category_path, prefabs in items_by_category.items():
            parent_item = category_nodes.get(category_path)
            if not parent_item:
                continue
            for prefab_type in sorted(prefabs, key=lambda t: t.type_name):
                display_name = self._format_display_name(prefab_type.type_name)
                if prefab_type.is_built_in:
                    display_name += " (Built-in)"
                tooltip = f"Name: {prefab_type.type_name}\nPath: {prefab_type.file_path if prefab_type.file_path else 'Built-in'}"
                self._add_leaf_item(parent_item, display_name, "prefab", prefab_type.file_path, tooltip)

        # Add per-component default prefab leaves
        for category_path, components in component_defaults_by_category.items():
            parent_item = category_nodes.get(category_path)
            if not parent_item:
                continue
            for component_type in sorted(components, key=lambda t: t.type_name):
                base_node = self._base_node_for_component(component_type)
                display_name = self._format_display_name(component_type.type_name)
                tooltip = (f"Default prefab: {base_node} + {component_type.type_name}\n"
                           f"Creates a {base_node} node named '{component_type.type_name}' "
                           f"with a {component_type.type_name} component.")
                self._add_leaf_item(parent_item, display_name, "component_default",
                                    component_type.type_name, tooltip, base_node=base_node)

    def _on_search_changed(self, text):
        """Filter all trees based on search text"""
        search_text = text.lower()
        for tree in (self.all_tree, self.nodes_tree, self.prefabs_tree):
            self._filter_tree(tree, search_text)

    def _filter_tree(self, tree_widget, search_text):
        """Filter a tree widget based on search text"""
        if not search_text:
            self._show_all_items(tree_widget)
            return

        root = tree_widget.invisibleRootItem()
        for i in range(root.childCount()):
            self._filter_tree_recursive(root.child(i), search_text)

    def _filter_tree_recursive(self, item, search_text):
        """
        Recursively filter tree items based on search text.

        Returns True if this item or any of its children match the search.
        """
        value = item.data(0, self.VALUE_ROLE)
        is_match = False

        if not self._is_category(item):
            # This is a selectable leaf - match on its underlying value/type name
            match_text = str(value) if value is not None else item.text(0)
            is_match = search_text in match_text.lower()
        else:
            # This is a category - check all children
            for i in range(item.childCount()):
                if self._filter_tree_recursive(item.child(i), search_text):
                    is_match = True

        item.setHidden(not is_match)
        if is_match and self._is_category(item):
            item.setExpanded(True)

        return is_match

    def _expand_all(self, tree_widget):
        """Expand every category in a tree and set the open arrow icon"""
        root = tree_widget.invisibleRootItem()
        for i in range(root.childCount()):
            self._expand_all_recursive(root.child(i))

    def _expand_all_recursive(self, item):
        """Recursively expand categories and update their arrow icons"""
        if self._is_category(item):
            item.setExpanded(True)
            item.setIcon(0, self.open_icon)
        for i in range(item.childCount()):
            self._expand_all_recursive(item.child(i))

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

    def _apply_selection(self, item):
        """Record the selection from a leaf item, return True if selectable"""
        kind = item.data(0, self.KIND_ROLE)
        if kind is None:
            return False

        self.selection_kind = kind
        self.selected_value = item.data(0, self.VALUE_ROLE)
        self.selected_base_node = item.data(0, self.BASE_NODE_ROLE) if kind == "component_default" else None
        return True

    def _on_item_clicked(self, item, column):
        """Handle item click"""
        if self._apply_selection(item):
            self.ok_button.setEnabled(True)
        else:
            # It's a category - toggle expansion
            self._clear_selection()
            item.setExpanded(not item.isExpanded())

    def _on_item_double_clicked(self, item, column):
        """Handle item double-click"""
        if self._apply_selection(item):
            self.accept()

    def _clear_selection(self):
        """Clear the current selection"""
        self.selection_kind = None
        self.selected_value = None
        self.selected_base_node = None
        self.ok_button.setEnabled(False)

    def get_selection(self):
        """
        Get the selection as (mode, value).

        - ("node", type_name)
        - ("prefab", file_path)
        - ("component_default", (base_node_type, component_type_name))
        """
        if self.selection_kind == "node":
            return ("node", self.selected_value)
        elif self.selection_kind == "prefab":
            return ("prefab", self.selected_value)
        elif self.selection_kind == "component_default":
            return ("component_default", (self.selected_base_node, self.selected_value))
        return (None, None)

    def reload_types(self):
        """Reload all types from the editor bridge"""
        self._load_types()
