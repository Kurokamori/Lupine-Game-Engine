"""
File Browser Panel
Browse project file system
"""

from PyQt6.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QLineEdit,
                             QPushButton, QTreeWidget, QTreeWidgetItem, QMenu,
                             QInputDialog, QMessageBox, QDialog, QFormLayout,
                             QComboBox, QLabel, QDialogButtonBox, QAbstractItemView)
from PyQt6.QtCore import Qt, pyqtSignal, QFileSystemWatcher
from PyQt6.QtGui import QIcon, QAction
from pathlib import Path
import os
import re
import json
import uuid
from .base_panel import EditorPanel

try:
    from editor.widgets import asset_drag
except ImportError:
    from widgets import asset_drag


class AssetFileTree(QTreeWidget):
    """File tree that can be a drag source for project assets.

    Selected file items expose their ``res://`` paths through the canonical Lupine
    asset MIME (and a ``text/uri-list`` of local files), so they can be dropped onto
    inspector reference fields the way Unity drags assets from the Project window.
    """

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setDragEnabled(True)
        self.setDragDropMode(QAbstractItemView.DragDropMode.DragOnly)
        self.setSelectionMode(QAbstractItemView.SelectionMode.ExtendedSelection)

    def mimeData(self, items):
        res_paths = []
        for item in items:
            absolute = item.data(0, Qt.ItemDataRole.UserRole)
            if not absolute or os.path.isdir(absolute):
                continue
            res_paths.append(asset_drag.to_res_path(absolute))
        if not res_paths:
            return super().mimeData(items)
        return asset_drag.build_asset_mimedata(res_paths)


class LupineIgnore:
    """
    Gitignore-style pattern matcher for .lupineignore files.

    Supported patterns:
    - Basic glob: *.txt, temp*, file.ext
    - Directory patterns: logs/ (matches directory only)
    - Rooted patterns: /build (only matches at root)
    - Double star: **/*.log (matches in any subdirectory)
    - Negation: !important.txt (re-include previously ignored)
    - Comments: # this is a comment
    - Extensions: *.meta, *.tmp
    """

    # Default patterns that are always ignored
    DEFAULT_IGNORES = [
        '__pycache__/',
        '*.pyc',
        '.git/',
        '.svn/',
        '.hg/',
        '.lupine/',
        '.DS_Store',
        'Thumbs.db',
        'build/',
        'export/',
        'vcpkg_installed/',
        '*.meta',
    ]

    def __init__(self, root_path: Path):
        self.root_path = root_path
        self.patterns = []  # List of (pattern, is_negation, is_dir_only, regex)
        self._load_defaults()
        self._load_ignore_file()

    def _load_defaults(self):
        """Load default ignore patterns"""
        for pattern in self.DEFAULT_IGNORES:
            self._add_pattern(pattern)

    def _load_ignore_file(self):
        """Load patterns from .lupineignore file"""
        ignore_file = self.root_path / '.lupineignore'
        if ignore_file.exists():
            try:
                content = ignore_file.read_text(encoding='utf-8')
                for line in content.splitlines():
                    line = line.strip()
                    # Skip empty lines and comments
                    if not line or line.startswith('#'):
                        continue
                    self._add_pattern(line)
            except Exception:
                pass  # Silently ignore read errors

    def _add_pattern(self, pattern: str):
        """Parse and add a pattern"""
        is_negation = False
        is_dir_only = False
        is_rooted = False

        # Check for negation
        if pattern.startswith('!'):
            is_negation = True
            pattern = pattern[1:]

        # Check for directory-only pattern
        if pattern.endswith('/'):
            is_dir_only = True
            pattern = pattern[:-1]

        # Check for rooted pattern
        if pattern.startswith('/'):
            is_rooted = True
            pattern = pattern[1:]

        # Convert gitignore pattern to regex
        regex = self._pattern_to_regex(pattern, is_rooted)

        self.patterns.append((pattern, is_negation, is_dir_only, is_rooted, regex))

    def _pattern_to_regex(self, pattern: str, is_rooted: bool) -> re.Pattern:
        """Convert a gitignore-style pattern to a regex"""
        # Escape special regex characters except our glob chars
        regex_parts = []
        i = 0

        while i < len(pattern):
            c = pattern[i]

            if c == '*':
                # Check for **
                if i + 1 < len(pattern) and pattern[i + 1] == '*':
                    # Check for **/ or just **
                    if i + 2 < len(pattern) and pattern[i + 2] == '/':
                        regex_parts.append('(?:.*/)?')  # Match any path or nothing
                        i += 3
                        continue
                    else:
                        regex_parts.append('.*')  # Match anything
                        i += 2
                        continue
                else:
                    regex_parts.append('[^/]*')  # Match anything except /
            elif c == '?':
                regex_parts.append('[^/]')  # Match single char except /
            elif c == '/':
                regex_parts.append('/')
            elif c in '.^$+{}[]|()\\':
                regex_parts.append('\\' + c)
            else:
                regex_parts.append(c)
            i += 1

        regex_str = ''.join(regex_parts)

        # If pattern contains /, it should match the full path
        # If not rooted and no /, it can match anywhere in the path
        if '/' not in pattern and not is_rooted:
            regex_str = '(?:^|/)' + regex_str + '$'
        elif is_rooted:
            regex_str = '^' + regex_str + '$'
        else:
            regex_str = '(?:^|/)' + regex_str + '$'

        return re.compile(regex_str)

    def is_ignored(self, path: Path, is_dir: bool = None) -> bool:
        """
        Check if a path should be ignored.

        Args:
            path: Path to check (relative to root or absolute)
            is_dir: Whether the path is a directory (auto-detected if None)

        Returns:
            True if the path should be ignored
        """
        # Convert to relative path
        try:
            if path.is_absolute():
                rel_path = path.relative_to(self.root_path)
            else:
                rel_path = path
        except ValueError:
            return False  # Path not under root

        # Auto-detect directory status
        if is_dir is None:
            full_path = self.root_path / rel_path
            is_dir = full_path.is_dir() if full_path.exists() else False

        # Convert to forward slashes for matching
        path_str = str(rel_path).replace('\\', '/')

        ignored = False

        for pattern, is_negation, is_dir_only, is_rooted, regex in self.patterns:
            # Skip dir-only patterns for files
            if is_dir_only and not is_dir:
                continue

            # Check if pattern matches
            if regex.search(path_str):
                ignored = not is_negation

        return ignored

    def reload(self):
        """Reload ignore patterns from file"""
        self.patterns.clear()
        self._load_defaults()
        self._load_ignore_file()


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
            "mRuby Script (.rb)",
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
    shader_opened = pyqtSignal(str)  # Emits .lsh shader path to open in shader editor
    scene_opened = pyqtSignal(str)  # Emits scene path to open as new tab
    prefab_opened = pyqtSignal(str)  # Emits .prefab path to open as an editable tab
    archetype_definition_opened = pyqtSignal(str)  # Emits .archetype path to open in the schema editor
    archetype_instance_opened = pyqtSignal(str)  # Emits .ares path to open in the inspector
    animation_clip_opened = pyqtSignal(str)  # Emits .animclip path to open in the animation timeline
    animation_graph_opened = pyqtSignal(str)  # Emits .animgraph path to open in the blend tree editor
    theme_opened = pyqtSignal(str)  # Emits .uitheme/.palette path to open in the UI Theme editor

    def __init__(self, parent=None):
        super().__init__("File Browser", parent)
        self.setObjectName("FileBrowserPanel")
        self.project_root = None
        self.current_path = None
        # Set by the main editor after construction
        self.main_editor = parent
        self.editor_bridge = None
        self.ignore_patterns = None  # LupineIgnore instance
        self.file_watcher = QFileSystemWatcher()
        self.file_watcher.directoryChanged.connect(self._on_directory_changed)
        self.file_watcher.fileChanged.connect(self._on_file_changed)
    
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
        self.file_tree = AssetFileTree()
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

        # Initialize ignore patterns
        self.ignore_patterns = LupineIgnore(Path(self.project_root))

        # Watch the directory and .lupineignore file
        if self.file_watcher.directories():
            self.file_watcher.removePaths(self.file_watcher.directories())
        if self.file_watcher.files():
            self.file_watcher.removePaths(self.file_watcher.files())
        self.file_watcher.addPath(self.project_root)

        # Watch .lupineignore file if it exists
        ignore_file = Path(self.project_root) / '.lupineignore'
        if ignore_file.exists():
            self.file_watcher.addPath(str(ignore_file))

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
                # Check against ignore patterns
                if self.ignore_patterns and self.ignore_patterns.is_ignored(item, item.is_dir()):
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
            '.rb': '💎',
            '.scene': '🎬',
            '.prefab': '🧩',
            '.json': '📋',
            '.txt': '📄',
            '.md': '📝',
            '.glsl': '🎨',
            '.vert': '🎨',
            '.frag': '🎨',
            '.archetype': '🧬',
            '.ares': '📦',
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
            
            if ext == '.lsh':
                # Open in the dedicated shader editor
                self.shader_opened.emit(str(path))
            elif ext == '.animclip':
                # Open in the animation timeline editor
                self.animation_clip_opened.emit(str(path))
            elif ext == '.animgraph':
                # Open in the blend tree (animation tree) editor
                self.animation_graph_opened.emit(str(path))
            elif ext == '.archetype':
                # Open in the archetype schema editor
                self.archetype_definition_opened.emit(str(path))
            elif ext == '.ares':
                # Open the archetype instance in the inspector
                self.archetype_instance_opened.emit(str(path))
            elif ext == '.uitheme' or ext == '.palette':
                # Open in the UI Theme editor
                self.theme_opened.emit(str(path))
            elif ext in ['.py', '.lua', '.rb', '.txt', '.json', '.glsl', '.vert', '.frag', '.h', '.hpp', '.cpp', '.c', '.md']:
                # Open in script editor
                self.script_opened.emit(str(path))
            elif ext == '.scene':
                # Open as scene tab
                self.scene_opened.emit(str(path))
            elif ext == '.prefab':
                # Open as an editable prefab tab
                self.prefab_opened.emit(str(path))
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

        create_rb_action = QAction("mRuby Script", self)
        create_rb_action.triggered.connect(lambda: self._create_file_quick('.rb'))
        create_menu.addAction(create_rb_action)

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

        create_menu.addSeparator()

        create_uitheme_action = QAction("UI Theme", self)
        create_uitheme_action.triggered.connect(lambda: self._create_file_quick('.uitheme'))
        create_menu.addAction(create_uitheme_action)

        create_palette_action = QAction("Color Palette", self)
        create_palette_action.triggered.connect(lambda: self._create_file_quick('.palette'))
        create_menu.addAction(create_palette_action)

        # Archetype data assets
        self._populate_archetype_create_menu(create_menu)

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
                if extension in ['.py', '.lua', '.rb', '.txt', '.json', '.glsl', '.md']:
                    self.script_opened.emit(str(new_file))
                elif extension == '.scene':
                    self.scene_opened.emit(str(new_file))
                elif extension in ['.uitheme', '.palette']:
                    self.theme_opened.emit(str(new_file))
            except Exception as e:
                QMessageBox.warning(self, "Error", f"Failed to create file:\n{str(e)}")

    def _current_target_dir(self) -> Path:
        """Resolve the directory new files should be created in based on selection."""
        current_item = self.file_tree.currentItem()
        if current_item:
            file_path = current_item.data(0, Qt.ItemDataRole.UserRole)
            if file_path:
                path = Path(file_path)
                return path.parent if path.is_file() else path
        return Path(self.project_root)

    def _populate_archetype_create_menu(self, create_menu):
        """Add archetype creation entries to the 'Create New' submenu."""
        create_menu.addSeparator()

        new_def_action = QAction("New Archetype Definition…", self)
        new_def_action.triggered.connect(self._create_new_archetype_definition)
        create_menu.addAction(new_def_action)

        new_interface_action = QAction("New Interface Definition…", self)
        new_interface_action.triggered.connect(self._create_new_interface_definition)
        create_menu.addAction(new_interface_action)

        if not self.editor_bridge:
            return

        import json
        try:
            definitions = json.loads(self.editor_bridge.get_archetype_definitions())
        except Exception:
            definitions = []

        if not definitions:
            return

        create_menu.addSeparator()

        submenus = {}

        def get_submenu(menu_path: str):
            if not menu_path:
                menu_path = "Archetypes"
            parent = create_menu
            accumulated = ""
            for raw_part in menu_path.split('/'):
                part = raw_part.strip()
                if not part:
                    continue
                accumulated = part if not accumulated else accumulated + "/" + part
                existing = submenus.get(accumulated)
                if existing is None:
                    existing = parent.addMenu(part)
                    submenus[accumulated] = existing
                parent = existing
            return parent

        for definition in sorted(definitions,
                                 key=lambda d: (d.get("menu_path", ""), d.get("archetype_class", ""))):
            # Abstract base archetypes cannot be instantiated directly.
            if definition.get("abstract"):
                continue
            submenu = get_submenu(definition.get("menu_path", "Archetypes"))
            class_name = definition.get("archetype_class", "Archetype")
            action = QAction(class_name, self)
            description = definition.get("description", "")
            if description:
                action.setToolTip(description)
            action.triggered.connect(
                lambda checked=False, d=definition: self._create_archetype_instance(d))
            submenu.addAction(action)

    def _create_archetype_instance(self, definition):
        """Create a new archetype instance (.ares) from a definition."""
        if not self.project_root or not self.editor_bridge:
            return

        class_name = definition.get("archetype_class", "Archetype")
        default_name = f"New{class_name}.ares"
        name, ok = QInputDialog.getText(
            self,
            f"New {class_name}",
            "Instance name:",
            QLineEdit.EchoMode.Normal,
            default_name
        )
        if not (ok and name):
            return

        if not name.lower().endswith('.ares'):
            name += '.ares'

        new_file = self._current_target_dir() / name
        try:
            created = self.editor_bridge.create_archetype_instance(class_name, str(new_file))
        except Exception as e:
            QMessageBox.warning(self, "Error", f"Failed to create archetype instance:\n{str(e)}")
            return

        if not created:
            QMessageBox.warning(self, "Error", f"Failed to create archetype instance '{name}'.")
            return

        self._refresh_tree()
        self.archetype_instance_opened.emit(str(new_file))

    def _create_new_archetype_definition(self):
        """Open the schema editor to create a new data-defined archetype (.archetype)."""
        if not self.project_root or not self.editor_bridge:
            return

        from dialogs import ArchetypeDefinitionDialog

        target_dir = self._current_target_dir()
        dialog = ArchetypeDefinitionDialog(self, default_location=str(target_dir),
                                           editor_bridge=self.editor_bridge)
        if dialog.exec() != QDialog.DialogCode.Accepted:
            return

        import json
        save_path = dialog.get_save_path()
        schema = dialog.get_schema_json()
        try:
            created = self.editor_bridge.create_archetype_definition(save_path, json.dumps(schema))
        except Exception as e:
            QMessageBox.warning(self, "Error", f"Failed to create archetype definition:\n{str(e)}")
            return

        if not created:
            QMessageBox.warning(self, "Error", "Failed to write archetype definition.")
            return

        self._refresh_tree()
        self.archetype_definition_opened.emit(save_path)

    def _create_new_interface_definition(self):
        """Open the schema editor to create a new data-defined interface (.interface)."""
        if not self.project_root or not self.editor_bridge:
            return

        from dialogs import InterfaceDefinitionDialog

        target_dir = self._current_target_dir()
        dialog = InterfaceDefinitionDialog(self, default_location=str(target_dir),
                                           editor_bridge=self.editor_bridge)
        if dialog.exec() != QDialog.DialogCode.Accepted:
            return

        import json
        save_path = dialog.get_save_path()
        schema = dialog.get_schema_json()
        try:
            created = self.editor_bridge.create_interface_definition(save_path, json.dumps(schema))
        except Exception as e:
            QMessageBox.warning(self, "Error", f"Failed to create interface definition:\n{str(e)}")
            return

        if not created:
            QMessageBox.warning(self, "Error", "Failed to write interface definition.")
            return

        self._refresh_tree()

    def _get_file_template(self, extension: str) -> str:
        """Get template content for file type"""
        templates = {
            '.py': '"""\nPython Script\n"""\n\n',
            '.lua': '-- Lua Script\n\n',
            '.rb': '# mRuby Script\n\n',
            '.scene': self._scene_template(),
            '.json': '{\n  \n}\n',
            '.txt': '',
            '.glsl': '// Shader\n\n',
            '.md': '# Document\n\n',
            '.uitheme': '{\n  "lupine_theme": 1,\n  "name": "Theme",\n  "extends": "",\n  "variables": {},\n  "font_roles": {},\n  "types": {}\n}\n',
            '.palette': '{\n  "lupine_palette": 1,\n  "name": "Palette",\n  "slots": []\n}\n',
        }
        return templates.get(extension, '')

    def _scene_template(self) -> str:
        """Build an empty scene that matches the engine's serialized format.

        Mirrors Scene::Serialize / Node::Serialize so a hand-authored new scene
        is structurally identical to one saved by the engine: a typed Scene with
        its own properties and a single typed Node root.
        """
        scene = {
            "type": "Scene",
            "properties": {
                "name": "New Scene"
            },
            "root": {
                "type": "Node",
                "properties": {
                    "name": "New Scene",
                    "active": True,
                    "visible": True,
                    "unique_name_in_owner": False
                },
                "uuid": str(uuid.uuid4()),
                "components": [],
                "children": []
            },
            "lupine_scene_version": "0.1.0"
        }
        return json.dumps(scene, indent=2) + "\n"

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
        # Check if .lupineignore was created
        if self.project_root:
            ignore_file = Path(self.project_root) / '.lupineignore'
            if ignore_file.exists() and str(ignore_file) not in self.file_watcher.files():
                self.file_watcher.addPath(str(ignore_file))
                if self.ignore_patterns:
                    self.ignore_patterns.reload()

        self._refresh_tree()

    def _on_file_changed(self, path: str):
        """Handle file change from file watcher (e.g., .lupineignore)"""
        if path.endswith('.lupineignore'):
            # Reload ignore patterns and refresh
            if self.ignore_patterns:
                self.ignore_patterns.reload()
            self._refresh_tree()

            # Re-add file to watcher (Qt removes it after modification)
            if Path(path).exists():
                self.file_watcher.addPath(path)
