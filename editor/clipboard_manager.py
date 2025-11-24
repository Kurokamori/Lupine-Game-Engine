"""
Clipboard Manager
Handles copy/cut/paste operations for nodes and text with context awareness
"""

from PyQt6.QtGui import QGuiApplication


class ClipboardManager:
    """Manages clipboard operations for the editor"""

    def __init__(self):
        self.node_clipboard = []  # List of nodes (for multi-selection support)
        self.is_cut = False  # Track if nodes were cut (for potential move operation)
        self.qt_clipboard = QGuiApplication.clipboard()

    def copy_nodes(self, nodes):
        """Copy nodes to clipboard"""
        if not nodes:
            return

        # Convert single node to list
        if not isinstance(nodes, list):
            nodes = [nodes]

        self.node_clipboard = nodes
        self.is_cut = False

    def cut_nodes(self, nodes):
        """Cut nodes to clipboard (marks for potential deletion)"""
        if not nodes:
            return

        # Convert single node to list
        if not isinstance(nodes, list):
            nodes = [nodes]

        self.node_clipboard = nodes
        self.is_cut = True

    def paste_nodes(self, parent_node, editor_bridge, duplicate_func):
        """
        Paste nodes from clipboard as children of parent_node

        Args:
            parent_node: The parent node to paste under
            editor_bridge: The editor bridge instance
            duplicate_func: Function to duplicate a node (should handle recursive duplication)

        Returns:
            List of pasted nodes, or empty list if paste failed
        """
        if not self.node_clipboard or not parent_node or not editor_bridge:
            return []

        pasted_nodes = []

        try:
            for node in self.node_clipboard:
                # Duplicate the node (always duplicate, even for cut)
                # This preserves the original in case of errors
                duplicated = duplicate_func(node)
                if duplicated:
                    # Add to parent
                    parent_node.add_child(duplicated)
                    pasted_nodes.append(duplicated)

            # If this was a cut operation, we could delete the originals here
            # For now, we'll leave them (convert cut to copy behavior)
            # In the future, we could track original nodes and delete them after successful paste
            self.is_cut = False

            return pasted_nodes

        except Exception as e:
            print(f"Error pasting nodes: {str(e)}")
            return []

    def has_nodes(self):
        """Check if there are nodes in the clipboard"""
        return len(self.node_clipboard) > 0

    def clear_nodes(self):
        """Clear the node clipboard"""
        self.node_clipboard = []
        self.is_cut = False

    def copy_text(self, text):
        """Copy text to system clipboard"""
        self.qt_clipboard.setText(text)

    def paste_text(self):
        """Get text from system clipboard"""
        return self.qt_clipboard.text()

    def has_text(self):
        """Check if there's text in the system clipboard"""
        return bool(self.qt_clipboard.text())
