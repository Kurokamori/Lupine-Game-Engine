"""
File Browser Panel
Browse project file system
"""

from PyQt6.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QLineEdit,
                             QPushButton, QTreeWidget, QTreeWidgetItem, QMenu,
                             QInputDialog, QMessageBox, QDialog, QFormLayout,
                             QComboBox, QLabel, QDialogButtonBox)
from PyQt6.QtCore import Qt, pyqtSignal, QFileSystemWatcher
from PyQt6.QtGui import QIcon, QAction
from pathlib import Path
import os
from .base_panel import EditorPanel


class NewFileDialog(QDialog):
    """Dialog for creating a new file"""
    
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Create New File")
        self.setModal(True)
        self.setMinimumWidth(400)
        
        layout = QVBoxLayout()
        
        # Form layout
        form_layout = QFormLayout()
        
        self.name_edit = QLineEdit()
        self.name_edit.setPlaceholderText("Enter file name...")
        form_layout.addRow("File Name:", self.name_edit)
        
        self.type_combo = QComboBox()
        self.type_combo.addItems([
            "Python Script (.py)",
            "Lua Script (.lua)",
            "Scene (.scene)",
            "Text File (.txt)",
            "JSON (.json)",
            "Shader (.glsl)",
            "C++ Header (.h)",
            "C++ Source (.cpp)",
        ])
        self.type_combo.currentTextChanged.connect(self._on_type_changed)
        form_layout.addRow("File Type:", self.type_combo)
        
        layout.addLayout(form_layout)
        
        # Buttons
        buttons = QDialogButtonBox(
            QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel
        )
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)
        
        self.setLayout(layout)
    
    def _on_type_changed(self, text):
        """Auto-add extension when type changes"""
        current_name = self.name_edit.text()
        if current_name and '.' in current_name:
            # Remove old extension
            current_name = current_name.rsplit('.', 1)[0]
        
        # Extract extension from type
        extension = text.split('(')[1].split(')')[0]
        if current_name:
            self.name_edit.setText(f"{current_name}{extension}")
    
    def get_file_info(self):
        """Get the file name and extension"""
        name = self.name_edit.text()
        type_text = self.type_combo.currentText()
        extension = type_text.split('(')[1].split(')')[0]
        
        # Ensure name has extension
        if not name.endswith(extension):
            if '.' in name:
                name = name.rsplit('.', 1)[0] + extension
            else:
                name = name + extension
        
        return name


class FileBrowserPanel(EditorPanel):
    """File system browser panel"""
    
    # Signals
    file_selected = pyqtSignal(str)  # Emits selected file path
    file_opened = pyqtSignal(str)  # Emits file path to open (double-click)
    script_opened = pyqtSignal(str)  # Emits script path to open in script editor
    scene_opened = pyqtSignal(str)  # Emits scene path to open as new tab
    
    def __init__(self, parent=None):
        super().__init__("File Browser", parent)
        self.setObjectName("FileBrowserPanel")
        self.project_root = None
        self.current_path = None
        self.file_watcher = QFileSystemWatcher()
        self.file_watcher.directoryChanged.connect(self._on_directory_changed)
    
    def _setup_panel(self):
        """Setup file browser panel UI"""
        # Toolbar with action buttons
        toolbar_layout = QHBoxLayout()
        toolbar_layout.setContentsMargins(5, 5, 5, 5)
        toolbar_layout.setSpacing(5)
        
        new_folder_btn = QPushButton("📁 New Folder")
        new_folder_btn.setToolTip("Create new folder")
        new_folder_btn.clicked.connect(self._create_new_folder)
        toolbar_layout.addWidget(new_folder_btn)
        
        new_file_btn = QPushButton("📄 New File")
        new_file_btn.setToolTip("Create new file")
        new_file_btn.clicked.connect(self._create_new_file)
        toolbar_layout.addWidget(new_file_btn)
        
        toolbar_layout.addStretch()
        
        refresh_btn = QPushButton("⟳")
        refresh_btn.setFixedWidth(30)
        refresh_btn.setToolTip("Refresh")
        refresh_btn.clicked.connect(self._refresh_tree)
        toolbar_layout.addWidget(refresh_btn)
        
        # Path bar
        path_layout = QHBoxLayout()
        path_layout.setContentsMargins(5, 0, 5, 5)
        
        self.path_edit = QLineEdit()
        self.path_edit.setReadOnly(True)
        self.path_edit.setPlaceholderText("Project root...")
        path_layout.addWidget(self.path_edit)
        
        up_btn = QPushButton("↑")
        up_btn.setFixedWidth(30)
        up_btn.setToolTip("Go up one directory")
        up_btn.clicked.connect(self._go_up_directory)
        path_layout.addWidget(up_btn)
        
        # File tree
        self.file_tree = QTreeWidget()
        self.file_tree.setHeaderLabel("Project Files")
        self.file_tree.setContextMenuPolicy(Qt.ContextMenuPolicy.CustomContextMenu)
        self.file_tree.customContextMenuRequested.connect(self._show_context_menu)
        self.file_tree.itemDoubleClicked.connect(self._on_item_double_clicked)
        self.file_tree.itemClicked.connect(self._on_item_clicked)
        self.file_tree.setExpandsOnDoubleClick(False)  # Handle manually
        
        # Add to layout
        self.content_layout.addLayout(toolbar_layout)
        self.content_layout.addLayout(path_layout)
        self.content_layout.addWidget(self.file_tree)
    
    def set_project_root(self, project_path: str):
        """Set the project root directory"""
        if project_path.endswith('.lupine'):
            self.project_root = str(Path(project_path).parent)
        else:
            self.project_root = project_path
        
        self.current_path = self.project_root
        self.path_edit.setText(self.project_root)
        
        # Watch the directory
        if self.file_watcher.directories():
            self.file_watcher.removePaths(self.file_watcher.directories())
        self.file_watcher.addPath(self.project_root)
        
        self._populate_tree()
    
    def _populate_tree(self):
        """Populate the file tree"""
        if not self.project_root:
            return
        
        self.file_tree.clear()
        root_path = Path(self.project_root)
        
        # Add items recursively
        self._add_directory_items(root_path, self.file_tree.invisibleRootItem())
        
        # Expand the first level
        for i in range(self.file_tree.topLevelItemCount()):
            self.file_tree.topLevelItem(i).setExpanded(False)
    
    def _add_directory_items(self, directory: Path, parent_item: QTreeWidgetItem):
        """Recursively add directory items"""
        try:
            # Get all items and sort (directories first, then files)
            items = sorted(directory.iterdir(), key=lambda x: (not x.is_dir(), x.name.lower()))
            
            for item in items:
                # Skip hidden files and certain directories
                if item.name.startswith('.') or item.name in ['__pycache__', 'build', 'vcpkg_installed']:
                    continue
                
                tree_item = QTreeWidgetItem(parent_item)
                tree_item.setText(0, item.name)
                tree_item.setData(0, Qt.ItemDataRole.UserRole, str(item))
                
                if item.is_dir():
                    tree_item.setText(0, f"📁 {item.name}")
                    # Recursively add children for directories
                    self._add_directory_items(item, tree_item)
                else:
                    # Set icon based on file type
                    icon = self._get_file_icon(item.suffix)
                    tree_item.setText(0, f"{icon} {item.name}")
        
        except PermissionError:
            pass
    
    def _get_file_icon(self, extension: str) -> str:
        """Get icon for file type"""
        icons = {
            '.py': '🐍',
            '.lua': '🌙',
            '.scene': '🎬',
            '.json': '📋',
            '.txt': '📄',
            '.md': '📝',
            '.glsl': '🎨',
            '.vert': '🎨',
            '.frag': '🎨',
            '.h': '📘',
            '.hpp': '📘',
            '.cpp': '📘',
            '.c': '📘',
            '.png': '🖼️',
            '.jpg': '🖼️',
            '.jpeg': '🖼️',
            '.bmp': '🖼️',
            '.wav': '🔊',
            '.mp3': '🔊',
            '.ogg': '🔊',
        }
        return icons.get(extension.lower(), '📄')
    
    def _on_item_clicked(self, item: QTreeWidgetItem, column: int):
        """Handle item click"""
        file_path = item.data(0, Qt.ItemDataRole.UserRole)
        if file_path:
            path = Path(file_path)
            if path.is_file():
                self.file_selected.emit(str(path))
    
    def _on_item_double_clicked(self, item: QTreeWidgetItem, column: int):
        """Handle item double click"""
        file_path = item.data(0, Qt.ItemDataRole.UserRole)
        if not file_path:
            return
        
        path = Path(file_path)
        
        if path.is_dir():
            # Toggle expand/collapse directory
            item.setExpanded(not item.isExpanded())
        else:
            # Open file based on type
            ext = path.suffix.lower()
            
            if ext in ['.py', '.lua', '.txt', '.json', '.glsl', '.vert', '.frag', '.h', '.hpp', '.cpp', '.c', '.md']:
                # Open in script editor
                self.script_opened.emit(str(path))
            elif ext == '.scene':
                # Open as scene tab
                self.scene_opened.emit(str(path))
            else:
                # Generic file open
                self.file_opened.emit(str(path))
    
    def _show_context_menu(self, position):
        """Show context menu"""
        item = self.file_tree.itemAt(position)
        
        menu = QMenu(self)
        
        # New Folder action
        new_folder_action = QAction("New Folder", self)
        new_folder_action.triggered.connect(self._create_new_folder)
        menu.addAction(new_folder_action)
        
        # Create New submenu
        create_menu = menu.addMenu("Create New")
        
        create_py_action = QAction("Python Script", self)
        create_py_action.triggered.connect(lambda: self._create_file_quick('.py'))
        create_menu.addAction(create_py_action)
        
        create_lua_action = QAction("Lua Script", self)
        create_lua_action.triggered.connect(lambda: self._create_file_quick('.lua'))
        create_menu.addAction(create_lua_action)
        
        create_scene_action = QAction("Scene", self)
        create_scene_action.triggered.connect(lambda: self._create_file_quick('.scene'))
        create_menu.addAction(create_scene_action)
        
        create_menu.addSeparator()
        
        create_text_action = QAction("Text File", self)
        create_text_action.triggered.connect(lambda: self._create_file_quick('.txt'))
        create_menu.addAction(create_text_action)
        
        create_json_action = QAction("JSON File", self)
        create_json_action.triggered.connect(lambda: self._create_file_quick('.json'))
        create_menu.addAction(create_json_action)
        
        create_shader_action = QAction("Shader", self)
        create_shader_action.triggered.connect(lambda: self._create_file_quick('.glsl'))
        create_menu.addAction(create_shader_action)
        
        # If item is selected, add item-specific actions
        if item:
            file_path = item.data(0, Qt.ItemDataRole.UserRole)
            if file_path:
                path = Path(file_path)
                
                menu.addSeparator()
                
                if path.is_file():
                    open_action = QAction("Open", self)
                    open_action.triggered.connect(lambda: self._on_item_double_clicked(item, 0))
                    menu.addAction(open_action)
                
                menu.addSeparator()
                
                rename_action = QAction("Rename", self)
                rename_action.triggered.connect(lambda: self._rename_item(item))
                menu.addAction(rename_action)
                
                delete_action = QAction("Delete", self)
                delete_action.triggered.connect(lambda: self._delete_item(item))
                menu.addAction(delete_action)
                
                menu.addSeparator()
                
                properties_action = QAction("Properties", self)
                properties_action.triggered.connect(lambda: self._show_properties(item))
                menu.addAction(properties_action)
        
        menu.exec(self.file_tree.viewport().mapToGlobal(position))
    
    def _create_new_folder(self):
        """Create a new folder"""
        if not self.project_root:
            return
        
        name, ok = QInputDialog.getText(
            self,
            "New Folder",
            "Folder name:",
            QLineEdit.EchoMode.Normal,
            "NewFolder"
        )
        
        if ok and name:
            # Get current directory context
            current_item = self.file_tree.currentItem()
            if current_item:
                file_path = current_item.data(0, Qt.ItemDataRole.UserRole)
                if file_path:
                    path = Path(file_path)
                    if path.is_file():
                        parent_dir = path.parent
                    else:
                        parent_dir = path
                else:
                    parent_dir = Path(self.project_root)
            else:
                parent_dir = Path(self.project_root)
            
            new_folder = parent_dir / name
            try:
                new_folder.mkdir(parents=True, exist_ok=False)
                self._refresh_tree()
            except FileExistsError:
                QMessageBox.warning(self, "Error", f"Folder '{name}' already exists.")
            except Exception as e:
                QMessageBox.warning(self, "Error", f"Failed to create folder:\n{str(e)}")
    
    def _create_new_file(self):
        """Create a new file with dialog"""
        if not self.project_root:
            return
        
        dialog = NewFileDialog(self)
        if dialog.exec() == QDialog.DialogCode.Accepted:
            filename = dialog.get_file_info()
            
            # Get current directory context
            current_item = self.file_tree.currentItem()
            if current_item:
                file_path = current_item.data(0, Qt.ItemDataRole.UserRole)
                if file_path:
                    path = Path(file_path)
                    if path.is_file():
                        parent_dir = path.parent
                    else:
                        parent_dir = path
                else:
                    parent_dir = Path(self.project_root)
            else:
                parent_dir = Path(self.project_root)
            
            new_file = parent_dir / filename
            try:
                # Create file with template content
                template = self._get_file_template(new_file.suffix)
                new_file.write_text(template)
                self._refresh_tree()
                # Open the new file
                self.script_opened.emit(str(new_file))
            except Exception as e:
                QMessageBox.warning(self, "Error", f"Failed to create file:\n{str(e)}")
    
    def _create_file_quick(self, extension: str):
        """Quick create file with just name input"""
        if not self.project_root:
            return
        
        name, ok = QInputDialog.getText(
            self,
            f"New {extension[1:].upper()} File",
            "File name:",
            QLineEdit.EchoMode.Normal,
            f"new_file{extension}"
        )
        
        if ok and name:
            # Ensure extension
            if not name.endswith(extension):
                name += extension
            
            # Get current directory context
            current_item = self.file_tree.currentItem()
            if current_item:
                file_path = current_item.data(0, Qt.ItemDataRole.UserRole)
                if file_path:
                    path = Path(file_path)
                    if path.is_file():
                        parent_dir = path.parent
                    else:
                        parent_dir = path
                else:
                    parent_dir = Path(self.project_root)
            else:
                parent_dir = Path(self.project_root)
            
            new_file = parent_dir / name
            try:
                # Create file with template content
                template = self._get_file_template(extension)
                new_file.write_text(template)
                self._refresh_tree()
                # Open the new file
                if extension in ['.py', '.lua', '.txt', '.json', '.glsl', '.md']:
                    self.script_opened.emit(str(new_file))
                elif extension == '.scene':
                    self.scene_opened.emit(str(new_file))
            except Exception as e:
                QMessageBox.warning(self, "Error", f"Failed to create file:\n{str(e)}")
    
    def _get_file_template(self, extension: str) -> str:
        """Get template content for file type"""
        templates = {
            '.py': '"""\nPython Script\n"""\n\n',
            '.lua': '-- Lua Script\n\n',
            '.scene': '{\n  "name": "New Scene",\n  "nodes": []\n}\n',
            '.json': '{\n  \n}\n',
            '.txt': '',
            '.glsl': '// Shader\n\n',
            '.md': '# Document\n\n',
        }
        return templates.get(extension, '')
    
    def _rename_item(self, item: QTreeWidgetItem):
        """Rename file or folder"""
        file_path = item.data(0, Qt.ItemDataRole.UserRole)
        if not file_path:
            return
        
        path = Path(file_path)
        new_name, ok = QInputDialog.getText(
            self,
            "Rename",
            "New name:",
            QLineEdit.EchoMode.Normal,
            path.name
        )
        
        if ok and new_name and new_name != path.name:
            new_path = path.parent / new_name
            try:
                path.rename(new_path)
                self._refresh_tree()
            except Exception as e:
                QMessageBox.warning(self, "Error", f"Failed to rename:\n{str(e)}")
    
    def _delete_item(self, item: QTreeWidgetItem):
        """Delete file or folder"""
        file_path = item.data(0, Qt.ItemDataRole.UserRole)
        if not file_path:
            return
        
        path = Path(file_path)
        
        reply = QMessageBox.question(
            self,
            "Confirm Delete",
            f"Are you sure you want to delete '{path.name}'?",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
            QMessageBox.StandardButton.No
        )
        
        if reply == QMessageBox.StandardButton.Yes:
            try:
                if path.is_dir():
                    import shutil
                    shutil.rmtree(path)
                else:
                    path.unlink()
                self._refresh_tree()
            except Exception as e:
                QMessageBox.warning(self, "Error", f"Failed to delete:\n{str(e)}")
    
    def _show_properties(self, item: QTreeWidgetItem):
        """Show file/folder properties"""
        file_path = item.data(0, Qt.ItemDataRole.UserRole)
        if not file_path:
            return
        
        path = Path(file_path)
        stat = path.stat()
        
        info = f"Name: {path.name}\n"
        info += f"Type: {'Directory' if path.is_dir() else 'File'}\n"
        info += f"Size: {stat.st_size} bytes\n"
        info += f"Path: {path}\n"
        
        QMessageBox.information(self, "Properties", info)
    
    def _go_up_directory(self):
        """Navigate up one directory"""
        # For now, just refresh to root
        if self.project_root:
            self._refresh_tree()
    
    def _refresh_tree(self):
        """Refresh the file tree"""
        self._populate_tree()
    
    def _on_directory_changed(self, path: str):
        """Handle directory change from file watcher"""
        self._refresh_tree()
