"""
Scene Tree Panel
Displays and manages the scene hierarchy (Godot-style)
"""

from PyQt6.QtWidgets import (QTreeWidget, QTreeWidgetItem, QVBoxLayout, QHBoxLayout,
                             QPushButton, QMenu, QMessageBox, QCheckBox, QWidget, QLabel, 
                             QStyledItemDelegate, QStyle)
from PyQt6.QtCore import Qt, pyqtSignal, QPoint, QRect
from PyQt6.QtGui import QAction, QIcon, QDropEvent, QPixmap, QPainter
from .base_panel import EditorPanel
from dialogs import AddNodeDialog, AddComponentDialog, AddNodePrefabDialog
import lupine_engine as le
from pathlib import Path
import os
import sys
import json

# Add parent directory to path to import theme
sys.path.insert(0, str(Path(__file__).parent.parent))
from theme import get_theme_manager, get_icon_pack_manager
from clipboard_manager import ClipboardManager

class IconButton(QPushButton):
    """Small icon button for tree items"""
    def __init__(self, icon_path, node_uuid_str, parent=None):
        super().__init__(parent)
        self.node_uuid_str = node_uuid_str  # Store node UUID
        self.setFixedSize(20, 20)
        self.setFlat(True)
        self.setFocusPolicy(Qt.FocusPolicy.NoFocus)  # Prevent stealing focus
        if os.path.exists(icon_path):
            self.setIcon(QIcon(icon_path))
            self.setIconSize(self.size())
        self.setStyleSheet("""
            QPushButton {
                border: none;
                background: transparent;
            }
            QPushButton:hover {
                background-color: rgba(255, 255, 255, 0.1);
                border-radius: 2px;
            }
        """)


class CenteredIconWidget(QWidget):
    """Widget that centers an icon button horizontally"""
    def __init__(self, button, parent=None):
        super().__init__(parent)
        layout = QHBoxLayout()
        layout.setContentsMargins(0, 0, 0, 0)
        layout.addStretch()
        layout.addWidget(button)
        layout.addStretch()
        self.setLayout(layout)
        self.button = button


class SceneTreeWidget(QTreeWidget):
    """Custom tree widget with drag-and-drop reparenting support"""

    node_reparented = pyqtSignal(str, str)  # dragged_node_uuid, new_parent_uuid

    def __init__(self, parent=None):
        super().__init__(parent)

    def mouseDoubleClickEvent(self, event):
        item = self.itemAt(event.position().toPoint())
        if item and self.columnAt(int(event.position().x())) == 0:
            self.editItem(item, 0)
        else:
            super().mouseDoubleClickEvent(event)

    def dropEvent(self, event: QDropEvent):
        """Handle drop event for reparenting nodes"""
        # Get the dragged item
        dragged_item = self.currentItem()
        if not dragged_item:
            event.ignore()
            return

        # Get the target item (where it was dropped)
        drop_indicator = self.dropIndicatorPosition()
        target_item = self.itemAt(event.position().toPoint())

        # Determine the new parent based on drop position
        new_parent_item = None
        if drop_indicator == QTreeWidget.DropIndicatorPosition.OnItem:
            # Dropped ON an item - make it a child
            new_parent_item = target_item
        elif drop_indicator in (QTreeWidget.DropIndicatorPosition.AboveItem,
                                 QTreeWidget.DropIndicatorPosition.BelowItem):
            # Dropped above/below - use the target's parent
            new_parent_item = target_item.parent() if target_item else None

        # If no parent item, we're dropping at root level
        if not new_parent_item:
            # Get the root item (first top-level item)
            new_parent_item = self.topLevelItem(0)

        # Make sure we're not trying to reparent to self or a descendant
        if dragged_item == new_parent_item or self._is_descendant(new_parent_item, dragged_item):
            event.ignore()
            return

        # Get UUIDs from items
        dragged_uuid = dragged_item.data(0, Qt.ItemDataRole.UserRole)
        new_parent_uuid = new_parent_item.data(0, Qt.ItemDataRole.UserRole) if new_parent_item else None

        if dragged_uuid and new_parent_uuid:
            # Emit signal for reparenting
            self.node_reparented.emit(dragged_uuid, new_parent_uuid)
            event.accept()
        else:
            event.ignore()

    def _is_descendant(self, potential_descendant, ancestor):
        """Check if potential_descendant is a child/grandchild/etc of ancestor"""
        if not potential_descendant:
            return False

        current = potential_descendant.parent()
        while current:
            if current == ancestor:
                return True
            current = current.parent()
        return False


class SceneTreePanel(EditorPanel):
    """Scene hierarchy tree panel (Godot-style)"""
    def _on_item_renamed(self, item, column):
        if column != 0:
            return
        node_uuid_str = item.data(0, Qt.ItemDataRole.UserRole)
        new_name = item.text(0)
        if not node_uuid_str or not new_name or not self.editor_bridge:
            return
        node_uuid = le.UUID.from_string(node_uuid_str)
        node = self.editor_bridge.get_node(node_uuid)
        if node and node.get_name() != new_name:
            node.set_name(new_name)
            self.editor_bridge.mark_scene_dirty()
            self.refresh_tree()
            if self.main_editor and 'console' in self.main_editor.panels:
                self.main_editor.panels['console'].log_message(
                    f"Renamed node to '{new_name}'", "Info")
    

    # Signals
    node_selected = pyqtSignal(object)  # Emits selected node
    node_deleted = pyqtSignal(object)   # Emits deleted node

    def __init__(self, parent=None):
        super().__init__("Scene Tree", parent)
        self.setObjectName("SceneTreePanel")
        self.main_editor = parent
        self.editor_bridge = None  # Will be set by main editor
        self.current_scene = None
        self.node_to_item_map = {}  # Maps node UUID to tree item
        self.clipboard_manager = ClipboardManager()

    def _setup_panel(self):
        """Setup scene tree panel UI"""
        # Get icon pack path from current theme
        theme_manager = get_theme_manager()
        icon_pack_manager = get_icon_pack_manager()
        current_theme = theme_manager.get_current_theme()
        icon_pack_name = current_theme.colors.icon_pack if current_theme else "dark"
        self.assets_path = Path(icon_pack_manager.get_icon_pack_path(icon_pack_name))
        
        # If icon pack path doesn't exist, fall back to dark
        if not self.assets_path.exists():
            self.assets_path = Path(__file__).parent.parent / "assets" / "icons" / "dark"
        
        # Toolbar
        toolbar_layout = QHBoxLayout()
        toolbar_layout.setContentsMargins(5, 5, 5, 5)

        add_node_btn = QPushButton("+ Add")
        add_node_btn.setToolTip("Add a new node to the scene")
        add_node_btn.clicked.connect(self._on_add_node_clicked)
        toolbar_layout.addWidget(add_node_btn)

        refresh_btn = QPushButton("⟳")
        refresh_btn.setToolTip("Refresh scene tree")
        refresh_btn.clicked.connect(self.refresh_tree)
        toolbar_layout.addWidget(refresh_btn)

        toolbar_layout.addStretch()

        # Tree widget (custom widget with drag-drop reparenting)
        self.tree_widget = SceneTreeWidget()
        self.tree_widget.setColumnCount(3)  # Name, View, Enable
        self.tree_widget.setHeaderLabels(["Scene", "View", "Enable"])
        
        # Enable multi-selection with Ctrl/Shift
        self.tree_widget.setSelectionMode(QTreeWidget.SelectionMode.ExtendedSelection)
        
        # Set column widths - make icon columns narrow and fixed
        self.tree_widget.setColumnWidth(0, 200)  # Name column
        self.tree_widget.setColumnWidth(1, 35)   # View column - narrower
        self.tree_widget.setColumnWidth(2, 50)   # Enable column - narrower
        
        # Anchor icon columns to the right
        header = self.tree_widget.header()
        header.setStretchLastSection(False)
        header.setSectionResizeMode(0, header.ResizeMode.Stretch)  # Name stretches
        header.setSectionResizeMode(1, header.ResizeMode.Fixed)     # View fixed
        header.setSectionResizeMode(2, header.ResizeMode.Fixed)     # Enable fixed
        
        self.tree_widget.setContextMenuPolicy(Qt.ContextMenuPolicy.CustomContextMenu)
        self.tree_widget.customContextMenuRequested.connect(self._show_context_menu)
        self.tree_widget.itemClicked.connect(self._on_item_clicked)
        self.tree_widget.itemSelectionChanged.connect(self._on_selection_changed)
        self.tree_widget.node_reparented.connect(self._on_node_reparented)
        self.tree_widget.itemChanged.connect(self._on_item_renamed)

        # Enable drag and drop for reparenting
        self.tree_widget.setDragEnabled(True)
        self.tree_widget.setAcceptDrops(True)
        self.tree_widget.setDragDropMode(QTreeWidget.DragDropMode.InternalMove)
        self.tree_widget.setDropIndicatorShown(True)

        # Set stylesheet for selection highlighting
        self.tree_widget.setStyleSheet("""
            QTreeWidget::item:selected {
                background-color: #3a5f8f;
                color: white;
            }
            QTreeWidget::item:hover {
                background-color: #2a4f7f;
            }
        """)

        # Add to layout
        self.content_layout.addLayout(toolbar_layout)
        self.content_layout.addWidget(self.tree_widget)

    def set_scene(self, scene):
        """Set the active scene and refresh the tree"""
        self.current_scene = scene
        self.refresh_tree()

    def refresh_tree(self):
        """Refresh the scene tree from the current scene"""
        self.tree_widget.clear()
        self.node_to_item_map.clear()

        if not self.current_scene or not self.editor_bridge:
            return

        # Get root node from the scene
        root_node = self.current_scene.get_root()
        if root_node:
            self._add_node_to_tree(root_node, None)
            self.tree_widget.expandAll()

    def _add_node_to_tree(self, node, parent_item):
        """Recursively add a node and its children to the tree"""
        # Create tree item
        if parent_item:
            item = QTreeWidgetItem(parent_item)
        else:
            item = QTreeWidgetItem(self.tree_widget)

        # Set node name in first column
        item.setText(0, node.get_name())
        item.setFlags(item.flags() | Qt.ItemFlag.ItemIsEditable)

        # Store node UUID in item data
        item.setData(0, Qt.ItemDataRole.UserRole, node.get_uuid().to_string())

        # Add to mapping
        self.node_to_item_map[node.get_uuid().to_string()] = item

        # Store node UUID string for button callbacks
        node_uuid_str = node.get_uuid().to_string()
        
        # Determine visibility and enabled states with parent checking
        visible_state = self._get_visibility_state(node)
        enabled_state = self._get_enabled_state(node)
        
        # Create and add visibility icon button (centered)
        view_btn = self._create_icon_button(visible_state, 'visible', node_uuid_str)
        view_btn.clicked.connect(lambda checked, uuid=node_uuid_str: self._toggle_visibility(uuid))
        view_widget = CenteredIconWidget(view_btn)
        self.tree_widget.setItemWidget(item, 1, view_widget)
        
        # Create and add enabled icon button (centered)
        enable_btn = self._create_icon_button(enabled_state, 'enabled', node_uuid_str)
        enable_btn.clicked.connect(lambda checked, uuid=node_uuid_str: self._toggle_enabled(uuid))
        enable_widget = CenteredIconWidget(enable_btn)
        self.tree_widget.setItemWidget(item, 2, enable_widget)

        # Add child nodes
        children = node.get_children()
        for child in children:
            self._add_node_to_tree(child, item)

        return item
    
    def _get_visibility_state(self, node):
        """Get visibility state: combines node state with parent state
        Returns: 'visible', 'invisible', 'visible-parent-invisible', 'invisible-parent-invisible'
        """
        # Check if any parent is invisible
        parent_invisible = False
        parent = node.get_parent()
        while parent:
            if not parent.is_visible():
                parent_invisible = True
                break
            parent = parent.get_parent()
        
        # Get this node's visibility
        node_visible = node.is_visible()
        
        # Return combined state
        if parent_invisible:
            return 'visible-parent-invisible' if node_visible else 'invisible-parent-invisible'
        else:
            return 'visible' if node_visible else 'invisible'
    
    def _get_enabled_state(self, node):
        """Get enabled state: combines node state with parent state
        Returns: 'enabled', 'disabled', 'enabled-parent-disabled', 'disabled-parent-disabled'
        """
        # Check if any parent is disabled
        parent_disabled = False
        parent = node.get_parent()
        while parent:
            if not parent.is_active():
                parent_disabled = True
                break
            parent = parent.get_parent()
        
        # Get this node's enabled state
        node_enabled = node.is_active()
        
        # Return combined state
        if parent_disabled:
            return 'enabled-parent-disabled' if node_enabled else 'disabled-parent-disabled'
        else:
            return 'enabled' if node_enabled else 'disabled'
    
    def _create_icon_button(self, state, state_type, node_uuid_str):
        """Create an icon button for the given state
        state: visibility or enabled state (with optional parent state)
        state_type: 'visible' or 'enabled'
        node_uuid_str: UUID string of the node
        """
        if state_type == 'visible':
            if state == 'visible':
                icon_name = 'visible.png'
            elif state == 'invisible':
                icon_name = 'invisible.png'
            elif state == 'visible-parent-invisible':
                icon_name = 'visible-parent-invisible.png'
            else:  # invisible-parent-invisible
                icon_name = 'invisible-parent-invisible.png'
        else:  # enabled
            if state == 'enabled':
                icon_name = 'enabled.png'
            elif state == 'disabled':
                icon_name = 'disabled.png'
            elif state == 'enabled-parent-disabled':
                icon_name = 'enabled-parent-disabled.png'
            else:  # disabled-parent-disabled
                icon_name = 'disabled-parent-disabled.png'
        
        icon_path = str(self.assets_path / icon_name)
        button = IconButton(icon_path, node_uuid_str)
        button.setToolTip(state.replace('-', ' ').title())
        return button
    
    def _toggle_visibility(self, node_uuid_str):
        """Toggle node visibility"""
        if not self.editor_bridge:
            return
        
        try:
            node_uuid = le.UUID.from_string(node_uuid_str)
            node = self.editor_bridge.get_node(node_uuid)
            if not node:
                return

            # Save scroll position
            scroll_bar = self.tree_widget.verticalScrollBar()
            scroll_value = scroll_bar.value()

            # Toggle visibility
            node.set_visible(not node.is_visible())
            self.editor_bridge.mark_scene_dirty()
            self.refresh_tree()

            # Restore scroll position
            scroll_bar.setValue(scroll_value)

            if self.main_editor and 'console' in self.main_editor.panels:
                state = "visible" if node.is_visible() else "invisible"
                self.main_editor.panels['console'].log_message(
                    f"Node '{node.get_name()}' is now {state}", "Info")
        except Exception as e:
            if self.main_editor and 'console' in self.main_editor.panels:
                self.main_editor.panels['console'].log_message(
                    f"Error toggling visibility: {str(e)}", "Error")
    
    def _toggle_enabled(self, node_uuid_str):
        """Toggle node enabled state"""
        if not self.editor_bridge:
            return
        
        try:
            node_uuid = le.UUID.from_string(node_uuid_str)
            node = self.editor_bridge.get_node(node_uuid)
            if not node:
                return

            # Save scroll position
            scroll_bar = self.tree_widget.verticalScrollBar()
            scroll_value = scroll_bar.value()

            # Toggle the active state
            node.set_active(not node.is_active())
            self.editor_bridge.mark_scene_dirty()
            self.refresh_tree()

            # Restore scroll position
            scroll_bar.setValue(scroll_value)

            if self.main_editor and 'console' in self.main_editor.panels:
                state = "enabled" if node.is_active() else "disabled"
                self.main_editor.panels['console'].log_message(
                    f"Node '{node.get_name()}' is now {state}", "Info")
        except Exception as e:
            if self.main_editor and 'console' in self.main_editor.panels:
                self.main_editor.panels['console'].log_message(
                    f"Error toggling enabled state: {str(e)}", "Error")

    def _on_item_clicked(self, item, column):
        """Handle item click - only select node when clicking the name column"""
        # Only process clicks on column 0 (name column)
        # Columns 1 and 2 are handled by the icon buttons directly
        if column != 0:
            return
        
        node_uuid_str = item.data(0, Qt.ItemDataRole.UserRole)
        if node_uuid_str and self.editor_bridge:
            # Get the node from the editor bridge
            node_uuid = le.UUID.from_string(node_uuid_str)
            node = self.editor_bridge.get_node(node_uuid)
            if node:
                # Multi-selection support:
                # The QTreeWidget handles Shift/Ctrl selection automatically via its selection mode
                # We just emit the currently selected node for inspector/viewport updates
                # In the future, we could emit all selected nodes for batch operations
                self.node_selected.emit(node)

    def _on_selection_changed(self):
        """Handle selection change"""
        selected_items = self.tree_widget.selectedItems()
        if selected_items:
            item = selected_items[0]
            self._on_item_clicked(item, 0)

    def _on_node_reparented(self, dragged_uuid_str, new_parent_uuid_str):
        """Handle node reparenting from drag-and-drop"""
        if not self.editor_bridge:
            return

        # CRITICAL: Pause runtime and stop rendering during reparenting to avoid race conditions
        viewport = None
        runtime_was_paused = False
        runtime = None

        # Stop viewport rendering
        if self.main_editor and hasattr(self.main_editor, 'viewport'):
            viewport = self.main_editor.viewport
            if viewport and hasattr(viewport, 'stop_rendering'):
                print(f"[_on_node_reparented] Stopping viewport rendering")
                viewport.stop_rendering()

        # Pause runtime (physics and game updates)
        if self.main_editor and hasattr(self.main_editor, 'runtime'):
            runtime = self.main_editor.runtime
            if runtime and hasattr(runtime, 'is_paused') and hasattr(runtime, 'pause'):
                runtime_was_paused = runtime.is_paused()
                if not runtime_was_paused:
                    print(f"[_on_node_reparented] Pausing runtime (physics/updates)")
                    runtime.pause()

        try:
            # Get the nodes
            dragged_uuid = le.UUID.from_string(dragged_uuid_str)
            new_parent_uuid = le.UUID.from_string(new_parent_uuid_str)

            dragged_node = self.editor_bridge.get_node(dragged_uuid)
            new_parent_node = self.editor_bridge.get_node(new_parent_uuid)

            if not dragged_node or not new_parent_node:
                QMessageBox.warning(self, "Error", "Failed to find nodes for reparenting")
                return

            # Perform the reparenting using the bridge method
            print(f"[_on_node_reparented] Reparenting '{dragged_node.get_name()}'")
            self.editor_bridge.reparent_node(dragged_node, new_parent_node)

            # Mark scene as dirty
            self.editor_bridge.mark_scene_dirty()

            # Refresh the tree
            self.refresh_tree()

            if self.main_editor and 'console' in self.main_editor.panels:
                self.main_editor.panels['console'].log_message(
                    f"Reparented '{dragged_node.get_name()}' to '{new_parent_node.get_name()}'", "Info")
            print(f"[_on_node_reparented] Reparenting completed successfully")

        except Exception as e:
            QMessageBox.critical(self, "Error", f"Error reparenting node: {str(e)}")
        finally:
            # CRITICAL: Always resume runtime and restart rendering, even if reparenting failed

            # Resume runtime if we paused it
            if runtime and not runtime_was_paused and hasattr(runtime, 'resume'):
                print(f"[_on_node_reparented] Resuming runtime (physics/updates)")
                runtime.resume()

            # Restart viewport rendering
            if viewport and hasattr(viewport, 'start_rendering'):
                print(f"[_on_node_reparented] Restarting viewport rendering")
                viewport.start_rendering()

    def _show_context_menu(self, position):
        """Show context menu for tree items"""
        menu = QMenu()

        # Get item at click position
        item = self.tree_widget.itemAt(position)
        # Do NOT set current item to preserve multiselection

        add_child_action = QAction("Add Child Node", self)
        add_child_action.triggered.connect(self._on_add_node_clicked)
        menu.addAction(add_child_action)

        add_component_action = QAction("Add Component", self)
        add_component_action.triggered.connect(self._on_add_component_clicked)
        add_component_action.setEnabled(item is not None)
        menu.addAction(add_component_action)

        menu.addSeparator()

        # Copy/Cut/Paste actions
        copy_action = QAction("Copy", self)
        copy_action.triggered.connect(self.copy_selected_nodes)
        copy_action.setEnabled(item is not None)
        copy_action.setShortcut("Ctrl+C")
        menu.addAction(copy_action)

        cut_action = QAction("Cut", self)
        cut_action.triggered.connect(self.cut_selected_nodes)
        cut_action.setEnabled(item is not None)
        cut_action.setShortcut("Ctrl+X")
        menu.addAction(cut_action)

        paste_action = QAction("Paste", self)
        paste_action.triggered.connect(self.paste_nodes)
        paste_action.setEnabled(self.clipboard_manager.has_nodes() and item is not None)
        paste_action.setShortcut("Ctrl+V")
        menu.addAction(paste_action)

        menu.addSeparator()

        duplicate_action = QAction("Duplicate", self)
        duplicate_action.triggered.connect(self._on_duplicate_node_clicked)
        duplicate_action.setEnabled(item is not None)
        duplicate_action.setShortcut("Ctrl+D")
        menu.addAction(duplicate_action)

        delete_action = QAction("Delete", self)
        delete_action.triggered.connect(self._on_delete_node_clicked)
        delete_action.setEnabled(item is not None)
        delete_action.setShortcut("Delete")
        menu.addAction(delete_action)

        menu.exec(self.tree_widget.viewport().mapToGlobal(position))

    def _on_add_node_clicked(self):
        """Handle add node button click"""
        if not self.main_editor or not self.editor_bridge:
            QMessageBox.warning(self, "Error", "Editor bridge not initialized")
            return

        # Show the combined add node/prefab dialog
        dialog = AddNodePrefabDialog(self.editor_bridge, self)
        if dialog.exec():
            mode, value = dialog.get_selection()
            if mode == "node":
                self._create_and_add_node(value)
            elif mode == "prefab":
                self._instantiate_prefab(value)

    def _on_add_component_clicked(self):
        """Handle add component click"""
        if not self.main_editor or not self.editor_bridge:
            QMessageBox.warning(self, "Error", "Editor bridge not initialized")
            return

        # Get selected node from tree
        selected_item = self.tree_widget.currentItem()
        if not selected_item:
            QMessageBox.warning(self, "No Selection", "Please select a node first")
            return

        # Show the add component dialog
        dialog = AddComponentDialog(self.editor_bridge, self)
        if dialog.exec():
            component_type = dialog.get_selected_type()
            if component_type:
                self._add_component_to_selected_node(component_type)

    def _on_delete_node_clicked(self):
        """Handle delete node click"""
        selected_items = self.tree_widget.selectedItems()
        if not selected_items:
            QMessageBox.warning(self, "No Selection", "Please select node(s) to delete")
            return

        # Confirm deletion for all selected nodes
        node_names = [self.editor_bridge.get_node(le.UUID.from_string(item.data(0, Qt.ItemDataRole.UserRole))).get_name() for item in selected_items if item.data(0, Qt.ItemDataRole.UserRole)]
        reply = QMessageBox.question(
            self,
            "Delete Node(s)",
            f"Are you sure you want to delete the following node(s) and all their children?\n\n" + "\n".join(node_names),
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
        )

        if reply == QMessageBox.StandardButton.Yes:
            # Save scroll position
            scroll_bar = self.tree_widget.verticalScrollBar()
            scroll_value = scroll_bar.value()

            deleted_names = []
            for item in selected_items:
                node_uuid_str = item.data(0, Qt.ItemDataRole.UserRole)
                if not node_uuid_str:
                    continue
                node_uuid = le.UUID.from_string(node_uuid_str)
                node = self.editor_bridge.get_node(node_uuid)
                if not node:
                    continue
                if self.editor_bridge.remove_node(node):
                    self.editor_bridge.mark_scene_dirty()
                    self.node_deleted.emit(node)
                    deleted_names.append(node.get_name())
                else:
                    QMessageBox.critical(self, "Error", f"Failed to delete node '{node.get_name()}'")

            # Refresh tree
            self.refresh_tree()
            # Restore scroll position
            scroll_bar.setValue(scroll_value)

            if self.main_editor and 'console' in self.main_editor.panels and deleted_names:
                self.main_editor.panels['console'].log_message(
                    f"Deleted node(s): {', '.join(deleted_names)}", "Info")

    def _on_duplicate_node_clicked(self):
        """Handle duplicate node click"""
        selected_item = self.tree_widget.currentItem()
        if not selected_item:
            QMessageBox.warning(self, "No Selection", "Please select a node to duplicate")
            return

        # Get the node
        node_uuid_str = selected_item.data(0, Qt.ItemDataRole.UserRole)
        if not node_uuid_str:
            return

        node_uuid = le.UUID.from_string(node_uuid_str)
        node = self.editor_bridge.get_node(node_uuid)
        if not node:
            QMessageBox.warning(self, "Error", "Node not found")
            return

        # CRITICAL: Pause runtime and stop rendering during duplication to avoid race conditions
        viewport = None
        runtime_was_paused = False
        runtime = None

        # Stop viewport rendering
        if self.main_editor and hasattr(self.main_editor, 'viewport'):
            viewport = self.main_editor.viewport
            if viewport and hasattr(viewport, 'stop_rendering'):
                print(f"[_on_duplicate_node_clicked] Stopping viewport rendering")
                viewport.stop_rendering()

        # Pause runtime (physics and game updates)
        if self.main_editor and hasattr(self.main_editor, 'runtime'):
            runtime = self.main_editor.runtime
            if runtime and hasattr(runtime, 'is_paused') and hasattr(runtime, 'pause'):
                runtime_was_paused = runtime.is_paused()
                if not runtime_was_paused:
                    print(f"[_on_duplicate_node_clicked] Pausing runtime (physics/updates)")
                    runtime.pause()

        try:
            # Log initial state
            if self.main_editor and 'console' in self.main_editor.panels:
                root = self.current_scene.get_root() if self.current_scene else None
                self.main_editor.panels['console'].log_message(
                    f"BEFORE DUPLICATE: Scene root exists: {root is not None}", "Debug")

            # Get parent - need to get it as a shared_ptr through editor bridge
            parent_raw = node.get_parent()
            if not parent_raw:
                # If no parent, this is the root node - can't duplicate root directly
                QMessageBox.warning(self, "Cannot Duplicate", "Cannot duplicate the root node")
                return

            # Get parent as shared_ptr by finding it through the scene
            # We need to get the parent's UUID and then get it as a shared_ptr
            parent_uuid_str = None
            parent_item = selected_item.parent()
            if parent_item:
                parent_uuid_str = parent_item.data(0, Qt.ItemDataRole.UserRole)

            if not parent_uuid_str:
                QMessageBox.critical(self, "Error", "Failed to find parent node")
                return

            parent_uuid = le.UUID.from_string(parent_uuid_str)
            parent = self.editor_bridge.get_node(parent_uuid)
            if not parent:
                QMessageBox.critical(self, "Error", "Parent node not found")
                return

            # Duplicate the node directly without using commands
            # Just add it to the parent's children directly
            print(f"[_on_duplicate_node_clicked] About to call _duplicate_node_recursive for '{node.get_name()}'")
            if self.main_editor and 'console' in self.main_editor.panels:
                self.main_editor.panels['console'].log_message(
                    f"[DUPLICATE START] About to duplicate node '{node.get_name()}'", "Debug")
            duplicated_node = self._duplicate_node_recursive(node)
            print(f"[_on_duplicate_node_clicked] _duplicate_node_recursive returned: {duplicated_node}")
            if not duplicated_node:
                QMessageBox.critical(self, "Error", "Failed to duplicate node")
                return

            print(f"[_on_duplicate_node_clicked] About to add duplicated node to parent")
            if self.main_editor and 'console' in self.main_editor.panels:
                self.main_editor.panels['console'].log_message(
                    f"[DUPLICATE] Node duplicated, adding to parent...", "Debug")
            # Add the duplicated node directly to the parent (no command execution)
            parent.add_child(duplicated_node)
            print(f"[_on_duplicate_node_clicked] Duplicated node added to parent")

            # CRITICAL: Initialize the duplicated node and all its components
            # When adding nodes dynamically to an already-initialized scene, we must manually call OnReady
            print(f"[_on_duplicate_node_clicked] Calling OnReady on duplicated node")
            duplicated_node.on_ready()
            print(f"[_on_duplicate_node_clicked] OnReady called on duplicated node")

            if self.main_editor and 'console' in self.main_editor.panels:
                self.main_editor.panels['console'].log_message(
                    f"[DUPLICATE] Creating undo command...", "Debug")
            print(f"[_on_duplicate_node_clicked] About to create_add_node_command")
            # Create an AddNodeCommand for undo/redo support
            # We record it without executing since we already added the node
            add_cmd = self.editor_bridge.create_add_node_command(duplicated_node, parent)
            print(f"[_on_duplicate_node_clicked] create_add_node_command returned: {add_cmd}")
            if add_cmd:
                print(f"[_on_duplicate_node_clicked] About to record_command")
                self.editor_bridge.record_command(add_cmd)
                print(f"[_on_duplicate_node_clicked] record_command completed")

            if self.main_editor and 'console' in self.main_editor.panels:
                self.main_editor.panels['console'].log_message(
                    f"[DUPLICATE] Marking scene dirty...", "Debug")
            print(f"[_on_duplicate_node_clicked] About to mark_scene_dirty")
            # Mark scene as dirty
            self.editor_bridge.mark_scene_dirty()
            print(f"[_on_duplicate_node_clicked] mark_scene_dirty completed")

            if self.main_editor and 'console' in self.main_editor.panels:
                self.main_editor.panels['console'].log_message(
                    f"[DUPLICATE] About to refresh tree...", "Debug")
            print(f"[_on_duplicate_node_clicked] About to refresh_tree")
            # Refresh tree
            self.refresh_tree()
            print(f"[_on_duplicate_node_clicked] refresh_tree completed")

            if self.main_editor and 'console' in self.main_editor.panels:
                self.main_editor.panels['console'].log_message(
                    f"[DUPLICATE] Tree refreshed, selecting node...", "Debug")
            # Auto-select the duplicated node
            dup_uuid_str = duplicated_node.get_uuid().to_string()
            if dup_uuid_str in self.node_to_item_map:
                new_item = self.node_to_item_map[dup_uuid_str]
                self.tree_widget.setCurrentItem(new_item)
                self.tree_widget.scrollToItem(new_item)

            print(f"[_on_duplicate_node_clicked] About to log DUPLICATE COMPLETE")
            if self.main_editor and 'console' in self.main_editor.panels:
                self.main_editor.panels['console'].log_message(
                    f"[DUPLICATE COMPLETE] Node '{node.get_name()}' duplicated successfully", "Debug")

            print(f"[_on_duplicate_node_clicked] About to log final Info message")
            if self.main_editor and 'console' in self.main_editor.panels:
                self.main_editor.panels['console'].log_message(
                    f"Duplicated node '{node.get_name()}'", "Info")

            print(f"[_on_duplicate_node_clicked] Duplication completed successfully!")

        except Exception as e:
            if self.main_editor and 'console' in self.main_editor.panels:
                import traceback
                self.main_editor.panels['console'].log_message(
                    f"Exception during duplication: {str(e)}\n{traceback.format_exc()}", "Error")
            QMessageBox.critical(self, "Error", f"Error duplicating node: {str(e)}")
        finally:
            # CRITICAL: Always resume runtime and restart rendering, even if duplication failed

            # Resume runtime if we paused it
            if runtime and not runtime_was_paused and hasattr(runtime, 'resume'):
                print(f"[_on_duplicate_node_clicked] Resuming runtime (physics/updates)")
                runtime.resume()

            # Restart viewport rendering
            if viewport and hasattr(viewport, 'start_rendering'):
                print(f"[_on_duplicate_node_clicked] Restarting viewport rendering")
                viewport.start_rendering()

    def _duplicate_node_recursive_with_commands(self, node, composite_command):
        """Recursively duplicate a node and all its children, adding commands to composite"""
        if not node:
            return None

        # Get node type
        node_type = node.get_type_name()

        # Create new node of same type (this doesn't need undo - it's not in the scene yet)
        new_node = self.editor_bridge.create_node(node_type)
        if not new_node:
            return None

        # Copy the name with " (Copy)" suffix (direct set, no command needed)
        original_name = node.get_name()
        new_node.set_name(f"{original_name} (Copy)")

        # Copy ALL node properties (direct set, no commands - node not in scene yet)
        try:
            # Get all properties from original node as JSON
            node_properties_json_str = self.editor_bridge.get_node_properties(node)
            node_properties_data = json.loads(node_properties_json_str)

            # Extract the nested "properties" object if it exists
            if "properties" in node_properties_data and isinstance(node_properties_data["properties"], dict):
                node_properties = node_properties_data["properties"]
            else:
                # Fallback to direct properties
                node_properties = node_properties_data

            # Copy each node property directly (no commands needed - not in scene yet)
            for prop_name, prop_value in node_properties.items():
                # Skip UUID and name (name already set above, UUID must be unique)
                if prop_name in ['uuid', 'name']:
                    continue

                try:
                    # Convert property value to JSON string for setting
                    value_json_str = json.dumps(prop_value)
                    # Direct set without command since node isn't in scene yet
                    self.editor_bridge.set_node_property_direct(new_node, prop_name, value_json_str)
                except:
                    pass  # Silently ignore property copy failures

        except Exception as e:
            # If node property copying fails, fall back to basic properties
            new_node.set_visible(node.is_visible())
            new_node.set_active(node.is_active())

        # Copy all components and their properties (direct add via bridge - no commands yet)
        try:
            components = self.editor_bridge.get_components(node)
            for component in components:
                # Get component type
                component_type = component.get_type_name()

                # Create new component of same type
                new_component = self.editor_bridge.create_component(component_type)
                if not new_component:
                    continue

                # Get all properties from original component as JSON
                properties_json_str = self.editor_bridge.get_component_properties(component)
                component_data = json.loads(properties_json_str)

                # IMPORTANT: Deserialize properties BEFORE adding to node
                # When we add a component to a node that's already "Ready", it calls OnAwake/OnReady
                # We need properties to be set before those initialization methods run
                try:
                    # Deserialize sets all properties without triggering individual OnPropertyChanged calls
                    self.editor_bridge.deserialize_component_direct(new_component, json.dumps(component_data))
                except Exception as e:
                    # If Deserialize fails, fallback to property-by-property copying
                    # Extract the nested "properties" object
                    if "properties" in component_data and isinstance(component_data["properties"], dict):
                        properties = component_data["properties"]
                    else:
                        # Fallback to old format (direct properties)
                        properties = component_data

                    # Copy each property to the new component (direct set)
                    for prop_name, prop_value in properties.items():
                        # Skip UUID and enabled
                        if prop_name in ['uuid', 'enabled']:
                            continue

                        try:
                            # Convert property value back to JSON for setting
                            value_json_str = json.dumps(prop_value)
                            # Direct set without command
                            self.editor_bridge.set_component_property_direct(new_component, prop_name, value_json_str)
                        except Exception as ex:
                            pass  # Silently fail

                # Copy enabled state explicitly
                try:
                    new_component.set_enabled(component.is_enabled())
                except:
                    pass  # Silently ignore

                # Now add the component to the node (this will trigger OnAwake/OnReady with correct properties)
                self.editor_bridge.add_component_direct(new_node, new_component)

        except Exception as e:
            if self.main_editor and 'console' in self.main_editor.panels:
                self.main_editor.panels['console'].log_message(
                    f"Failed to copy components for node '{original_name}': {str(e)}",
                    "Error")

        # Recursively duplicate children
        children = node.get_children()
        for child in children:
            duplicated_child = self._duplicate_node_recursive_with_commands(child, composite_command)
            if duplicated_child:
                # Add child directly (no command - parent not in scene yet)
                new_node.add_child(duplicated_child)

        return new_node

    def _duplicate_node_recursive(self, node):
        """Recursively duplicate a node and all its children (legacy method for clipboard)"""
        if not node:
            return None

        # Get node type
        node_type = node.get_type_name()
        print(f"[_duplicate_node_recursive] START - Duplicating node '{node.get_name()}' of type {node_type}")

        # Create new node of same type
        new_node = self.editor_bridge.create_node(node_type)
        print(f"[_duplicate_node_recursive] Created new node of type {node_type}")
        if not new_node:
            return None

        # Copy the name with " (Copy)" suffix
        original_name = node.get_name()
        new_node.set_name(f"{original_name} (Copy)")

        # Copy ALL node properties (including transforms for Node2D/Node3D)
        # Use Deserialize to avoid creating individual commands for each property
        try:
            # Get all properties from original node as JSON
            node_properties_json_str = self.editor_bridge.get_node_properties(node)
            node_properties_data = json.loads(node_properties_json_str)

            # Extract the nested "properties" object if it exists
            if "properties" in node_properties_data and isinstance(node_properties_data["properties"], dict):
                node_properties = node_properties_data["properties"]
            else:
                # Fallback to direct properties
                node_properties = node_properties_data

            # Copy each property directly using the new direct setter
            for prop_name, prop_value in node_properties.items():
                # Skip UUID and name (name already set above, UUID must be unique)
                if prop_name in ['uuid', 'name']:
                    continue

                try:
                    # Convert property value to JSON string for setting
                    value_json_str = json.dumps(prop_value)
                    # Direct set without command
                    self.editor_bridge.set_node_property_direct(new_node, prop_name, value_json_str)
                except:
                    pass  # Silently ignore property copy failures

        except Exception as e:
            # If node property copying fails, fall back to basic properties
            new_node.set_visible(node.is_visible())
            new_node.set_active(node.is_active())

        # Copy all components and their properties
        # IMPORTANT: Create and prepare ALL components first, then add them all to the node
        # This ensures components that depend on each other (like CollisionMesh3D -> StaticBody3D)
        # can find each other during OnReady()
        try:
            components = self.editor_bridge.get_components(node)
            print(f"[_duplicate_node_recursive] Found {len(components)} components to copy")

            # Phase 1: Create all components and deserialize their properties
            new_components = []
            for component in components:
                # Get component type
                component_type = component.get_type_name()

                # Create new component of same type
                new_component = self.editor_bridge.create_component(component_type)
                if not new_component:
                    continue

                # Get all properties from original component as JSON
                properties_json_str = self.editor_bridge.get_component_properties(component)
                component_data = json.loads(properties_json_str)

                print(f"[_duplicate_node_recursive] Preparing component {component_type}")

                # Deserialize properties (but don't add to node yet)
                try:
                    print(f"[_duplicate_node_recursive] About to deserialize {component_type}")
                    self.editor_bridge.deserialize_component_direct(new_component, json.dumps(component_data))
                    print(f"[_duplicate_node_recursive] Successfully deserialized {component_type}")
                except Exception as e:
                    print(f"[_duplicate_node_recursive] Deserialize failed for {component_type}: {e}")
                    # If Deserialize fails, fallback to property-by-property copying
                    if "properties" in component_data and isinstance(component_data["properties"], dict):
                        properties = component_data["properties"]
                    else:
                        properties = component_data

                    for prop_name, prop_value in properties.items():
                        if prop_name in ['uuid', 'enabled']:
                            continue
                        try:
                            value_json_str = json.dumps(prop_value)
                            self.editor_bridge.set_component_property_direct(new_component, prop_name, value_json_str)
                        except:
                            pass

                # Copy enabled state
                try:
                    new_component.set_enabled(component.is_enabled())
                except:
                    pass

                new_components.append(new_component)

            # Phase 2: Add ALL components to the node at once
            # This way all components are present before any OnReady() calls
            print(f"[_duplicate_node_recursive] Adding {len(new_components)} components to node")
            for new_component in new_components:
                self.editor_bridge.add_component_direct(new_node, new_component)

        except Exception as e:
            if self.main_editor and 'console' in self.main_editor.panels:
                self.main_editor.panels['console'].log_message(
                    f"Failed to copy components for node '{original_name}': {str(e)}",
                    "Error")

        # Recursively duplicate children
        children = node.get_children()
        print(f"[_duplicate_node_recursive] About to duplicate {len(children)} children")
        for child in children:
            duplicated_child = self._duplicate_node_recursive(child)
            if duplicated_child:
                new_node.add_child(duplicated_child)

        print(f"[_duplicate_node_recursive] Returning duplicated node '{new_node.get_name()}'")
        return new_node

    def copy_selected_nodes(self):
        """Copy selected nodes to clipboard"""
        selected_items = self.tree_widget.selectedItems()
        if not selected_items:
            return

        # Get nodes from selected items
        nodes = []
        for item in selected_items:
            node_uuid_str = item.data(0, Qt.ItemDataRole.UserRole)
            if node_uuid_str:
                node_uuid = le.UUID.from_string(node_uuid_str)
                node = self.editor_bridge.get_node(node_uuid)
                if node:
                    nodes.append(node)

        if nodes:
            self.clipboard_manager.copy_nodes(nodes)
            if self.main_editor and 'console' in self.main_editor.panels:
                count = len(nodes)
                self.main_editor.panels['console'].log_message(
                    f"Copied {count} node{'s' if count > 1 else ''}", "Info")

    def cut_selected_nodes(self):
        """Cut selected nodes to clipboard"""
        selected_items = self.tree_widget.selectedItems()
        if not selected_items:
            return

        # Get nodes from selected items
        nodes = []
        for item in selected_items:
            node_uuid_str = item.data(0, Qt.ItemDataRole.UserRole)
            if node_uuid_str:
                node_uuid = le.UUID.from_string(node_uuid_str)
                node = self.editor_bridge.get_node(node_uuid)
                if node:
                    nodes.append(node)

        if nodes:
            self.clipboard_manager.cut_nodes(nodes)
            if self.main_editor and 'console' in self.main_editor.panels:
                count = len(nodes)
                self.main_editor.panels['console'].log_message(
                    f"Cut {count} node{'s' if count > 1 else ''}", "Info")

    def paste_nodes(self):
        """Paste nodes from clipboard as children of selected node"""
        if not self.clipboard_manager.has_nodes():
            return

        selected_item = self.tree_widget.currentItem()
        if not selected_item:
            QMessageBox.warning(self, "No Selection", "Please select a parent node to paste into")
            return

        # Get parent node
        node_uuid_str = selected_item.data(0, Qt.ItemDataRole.UserRole)
        if not node_uuid_str:
            return

        node_uuid = le.UUID.from_string(node_uuid_str)
        parent_node = self.editor_bridge.get_node(node_uuid)
        if not parent_node:
            QMessageBox.warning(self, "Error", "Parent node not found")
            return

        try:
            # Get nodes from clipboard
            nodes_to_paste = self.clipboard_manager.node_clipboard
            if not nodes_to_paste:
                return

            # Create a composite command for the entire paste operation
            count = len(nodes_to_paste)
            composite = self.editor_bridge.create_composite_command(
                f"Paste {count} Node{'s' if count > 1 else ''}"
            )

            pasted_nodes = []
            for node in nodes_to_paste:
                # Duplicate the node
                duplicated = self._duplicate_node_recursive_with_commands(node, composite)
                if duplicated:
                    # Add command to add the duplicated node to parent
                    add_cmd = self.editor_bridge.create_add_node_command(duplicated, parent_node)
                    if add_cmd:
                        composite.add_command(add_cmd)
                        pasted_nodes.append(duplicated)

            # Execute the composite command
            if pasted_nodes and self.editor_bridge.execute_command(composite):
                # Clear clipboard if this was a cut operation
                self.clipboard_manager.is_cut = False

                # Refresh tree
                self.refresh_tree()

                # Select the first pasted node
                if pasted_nodes:
                    first_uuid_str = pasted_nodes[0].get_uuid().to_string()
                    if first_uuid_str in self.node_to_item_map:
                        new_item = self.node_to_item_map[first_uuid_str]
                        self.tree_widget.setCurrentItem(new_item)
                        self.tree_widget.scrollToItem(new_item)

                if self.main_editor and 'console' in self.main_editor.panels:
                    self.main_editor.panels['console'].log_message(
                        f"Pasted {count} node{'s' if count > 1 else ''}", "Info")
            else:
                QMessageBox.warning(self, "Paste Failed", "Failed to paste nodes")

        except Exception as e:
            QMessageBox.critical(self, "Error", f"Error pasting nodes: {str(e)}")

    def _create_and_add_node(self, node_type):
        """Create a node of the specified type and add it to the scene"""
        if not self.main_editor or not self.editor_bridge:
            return

        try:
            # Create the node
            node = self.editor_bridge.create_node(node_type)
            if not node:
                QMessageBox.critical(self, "Error", f"Failed to create node of type: {node_type}")
                return

            # Check if scene is empty (no root node)
            # Use current_scene.get_root() instead of editor_bridge.get_root_node()
            root_node = self.current_scene.get_root() if self.current_scene else None

            if not root_node:
                # Empty scene - add this node as the root
                if self.current_scene:
                    self.current_scene.set_root(node)

                    # Mark scene as dirty
                    self.editor_bridge.mark_scene_dirty()

                    # Refresh the scene tree
                    self.refresh_tree()

                    # Auto-select the newly added root node
                    node_uuid_str = node.get_uuid().to_string()
                    if node_uuid_str in self.node_to_item_map:
                        new_item = self.node_to_item_map[node_uuid_str]
                        self.tree_widget.setCurrentItem(new_item)
                        self.tree_widget.scrollToItem(new_item)

                    if self.main_editor and 'console' in self.main_editor.panels:
                        self.main_editor.panels['console'].log_message(
                            f"Added root node '{node.get_name()}' of type '{node_type}'", "Info")
                else:
                    QMessageBox.critical(self, "Error", "No scene available")
                return

            # Determine parent node for non-empty scenes
            selected_item = self.tree_widget.currentItem()
            if selected_item:
                # Get selected node as parent
                node_uuid_str = selected_item.data(0, Qt.ItemDataRole.UserRole)
                node_uuid = le.UUID.from_string(node_uuid_str)
                parent_node = self.editor_bridge.get_node(node_uuid)
            else:
                # Use root node as parent
                parent_node = root_node

            if not parent_node:
                QMessageBox.critical(self, "Error", "No parent node available")
                return

            # Add the node to the scene
            if self.editor_bridge.add_node(node, parent_node):
                # Mark scene as dirty
                self.editor_bridge.mark_scene_dirty()

                # Refresh the scene tree
                self.refresh_tree()

                # Auto-select the newly added node
                node_uuid_str = node.get_uuid().to_string()
                if node_uuid_str in self.node_to_item_map:
                    new_item = self.node_to_item_map[node_uuid_str]
                    self.tree_widget.setCurrentItem(new_item)
                    self.tree_widget.scrollToItem(new_item)

                if self.main_editor and 'console' in self.main_editor.panels:
                    self.main_editor.panels['console'].log_message(
                        f"Added node '{node.get_name()}' of type '{node_type}'", "Info")
            else:
                QMessageBox.critical(self, "Error", "Failed to add node to scene")

        except Exception as e:
            QMessageBox.critical(self, "Error", f"Error creating node: {str(e)}")

    def _instantiate_prefab(self, prefab_path):
        """Instantiate a prefab and add it to the scene"""
        if not self.main_editor or not self.editor_bridge:
            return

        try:
            # Load the prefab
            prefab = self.editor_bridge.load_prefab(prefab_path)
            if not prefab:
                QMessageBox.critical(self, "Error", f"Failed to load prefab: {prefab_path}")
                return

            # Instantiate the prefab (returns a node)
            # Check if scene is empty (no root node)
            # Use current_scene.get_root() instead of editor_bridge.get_root_node()
            root_node = self.current_scene.get_root() if self.current_scene else None

            if not root_node:
                # Empty scene - instantiate prefab as root
                # Note: We need to instantiate without a parent first
                # For now, show an error - prefabs need a parent node structure
                QMessageBox.warning(self, "Empty Scene",
                                   "Please add a root node first before instantiating prefabs.\n"
                                   "Prefabs should be added to an existing scene structure.")
                return

            # Determine parent node for non-empty scenes
            selected_item = self.tree_widget.currentItem()
            if selected_item:
                node_uuid_str = selected_item.data(0, Qt.ItemDataRole.UserRole)
                node_uuid = le.UUID.from_string(node_uuid_str)
                parent_node = self.editor_bridge.get_node(node_uuid)
            else:
                parent_node = root_node

            if not parent_node:
                QMessageBox.critical(self, "Error", "No parent node available")
                return

            # Instantiate the prefab
            instance = self.editor_bridge.instantiate_prefab(prefab, parent_node)
            if instance:
                # Mark scene as dirty
                self.editor_bridge.mark_scene_dirty()

                # Refresh tree
                self.refresh_tree()

                if self.main_editor and 'console' in self.main_editor.panels:
                    self.main_editor.panels['console'].log_message(
                        f"Instantiated prefab '{prefab.get_name()}'", "Info")
            else:
                QMessageBox.critical(self, "Error", "Failed to instantiate prefab")

        except Exception as e:
            QMessageBox.critical(self, "Error", f"Error instantiating prefab: {str(e)}")

    def _add_component_to_selected_node(self, component_type):
        """Add a component to the selected node"""
        if not self.main_editor or not self.editor_bridge:
            return

        # Get selected item
        selected_item = self.tree_widget.currentItem()
        if not selected_item:
            return

        try:
            # Get the node
            node_uuid_str = selected_item.data(0, Qt.ItemDataRole.UserRole)
            node_uuid = le.UUID.from_string(node_uuid_str)
            node = self.editor_bridge.get_node(node_uuid)

            if not node:
                QMessageBox.critical(self, "Error", "Node not found")
                return

            # Create the component
            component = self.editor_bridge.create_component(component_type)
            if not component:
                QMessageBox.critical(self, "Error", f"Failed to create component of type: {component_type}")
                return

            # Add component to node
            if self.editor_bridge.add_component(node, component):
                # Mark scene as dirty
                self.editor_bridge.mark_scene_dirty()

                if self.main_editor and 'console' in self.main_editor.panels:
                    self.main_editor.panels['console'].log_message(
                        f"Added component '{component_type}' to node '{node.get_name()}'", "Info")
            else:
                QMessageBox.critical(self, "Error", "Failed to add component to node")

        except Exception as e:
            QMessageBox.critical(self, "Error", f"Error creating component: {str(e)}")

    def select_node(self, node):
        """Select a node in the tree (called from viewport picking)"""
        if not node:
            return
        
        # Find the item for this node
        node_uuid_str = node.get_uuid().to_string()
        if node_uuid_str in self.node_to_item_map:
            item = self.node_to_item_map[node_uuid_str]
            
            # Block signals to prevent circular updates
            self.tree_widget.blockSignals(True)
            self.tree_widget.setCurrentItem(item)
            self.tree_widget.scrollToItem(item)
            self.tree_widget.blockSignals(False)

    def clear_selection(self):
        """Clear the tree selection (called when clicking empty viewport space)"""
        self.tree_widget.blockSignals(True)
        self.tree_widget.clearSelection()
        self.tree_widget.blockSignals(False)
