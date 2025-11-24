"""
Asset Manager Tool
Comprehensive asset tracking, placeholder management, and import settings tool
"""

from PyQt6.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QLabel,
                             QPushButton, QTreeWidget, QTreeWidgetItem, QSplitter,
                             QTabWidget, QLineEdit, QTextEdit, QComboBox, QSpinBox,
                             QCheckBox, QFileDialog, QMessageBox, QDialog, QFormLayout,
                             QDialogButtonBox, QGroupBox, QListWidget, QListWidgetItem,
                             QToolButton, QMenu, QProgressBar, QFrame, QScrollArea,
                             QDoubleSpinBox, QTableWidget, QTableWidgetItem, QHeaderView,
                             QInputDialog)
from PyQt6.QtCore import Qt, pyqtSignal, QTimer, QSize, QFileSystemWatcher
from PyQt6.QtGui import QColor, QBrush, QIcon, QAction, QPixmap, QPainter, QFont
from pathlib import Path
import json
import sys
import re
import shutil
from datetime import datetime
from typing import Dict, List, Set, Optional

# Import base panel
editor_path = Path(__file__).parent.parent.parent
if str(editor_path) not in sys.path:
    sys.path.insert(0, str(editor_path))

from panels.base_panel import EditorPanel


def create_generic_icon(extension: str, size: int = 48) -> QIcon:
    """Create a generic file type icon"""
    pixmap = QPixmap(size, size)
    pixmap.fill(Qt.GlobalColor.transparent)

    painter = QPainter(pixmap)
    painter.setRenderHint(QPainter.RenderHint.Antialiasing)

    # Extension to color mapping
    color_map = {
        # Images
        '.png': '#4EC9B0', '.jpg': '#4EC9B0', '.jpeg': '#4EC9B0', '.bmp': '#4EC9B0',
        '.gif': '#4EC9B0', '.tga': '#4EC9B0', '.svg': '#4EC9B0',
        # Fonts
        '.ttf': '#DCDCAA', '.otf': '#DCDCAA',
        # Audio
        '.wav': '#C586C0', '.mp3': '#C586C0', '.ogg': '#C586C0',
        # 3D Models
        '.obj': '#569CD6', '.fbx': '#569CD6', '.gltf': '#569CD6', '.glb': '#569CD6',
        # Scripts
        '.lua': '#CE9178', '.py': '#CE9178', '.rb': '#CE9178', '.js': '#CE9178',
        # Data
        '.json': '#9CDCFE', '.xml': '#9CDCFE', '.yaml': '#9CDCFE', '.yml': '#9CDCFE',
        # Video
        '.mov': '#D16969', '.mp4': '#D16969', '.avi': '#D16969',
    }

    color = color_map.get(extension.lower(), '#7b5fb8')
    painter.setBrush(QColor(color))
    painter.setPen(Qt.PenStyle.NoPen)

    # Draw rounded rectangle
    rect_margin = int(size * 0.1)
    painter.drawRoundedRect(
        rect_margin, rect_margin,
        size - 2 * rect_margin, size - 2 * rect_margin,
        size * 0.1, size * 0.1
    )

    # Draw extension text
    if extension:
        painter.setPen(QColor('#ffffff'))
        font = QFont()
        font.setPixelSize(int(size * 0.25))
        font.setBold(True)
        painter.setFont(font)

        text = extension[1:].upper() if extension.startswith('.') else extension.upper()
        if len(text) > 4:
            text = text[:4]

        painter.drawText(
            rect_margin, rect_margin,
            size - 2 * rect_margin, size - 2 * rect_margin,
            Qt.AlignmentFlag.AlignCenter,
            text
        )

    painter.end()
    return QIcon(pixmap)


def create_thumbnail(file_path: Path, size: int = 48) -> QIcon:
    """Create a thumbnail for an image file"""
    try:
        pixmap = QPixmap(str(file_path))
        if not pixmap.isNull():
            # Scale to thumbnail size while maintaining aspect ratio
            scaled = pixmap.scaled(
                size, size,
                Qt.AspectRatioMode.KeepAspectRatio,
                Qt.TransformationMode.SmoothTransformation
            )

            # Create a square icon with the scaled image centered
            icon_pixmap = QPixmap(size, size)
            icon_pixmap.fill(Qt.GlobalColor.transparent)

            painter = QPainter(icon_pixmap)
            x = (size - scaled.width()) // 2
            y = (size - scaled.height()) // 2
            painter.drawPixmap(x, y, scaled)
            painter.end()

            return QIcon(icon_pixmap)
    except:
        pass

    # Fallback to generic icon
    return create_generic_icon(file_path.suffix)


class Asset:
    """Represents a project asset"""

    def __init__(self, name: str, path: str, exists: bool = False):
        self.name = name
        self.path = path  # Relative to project root
        self.exists = exists
        self.tags: Set[str] = set()
        self.status = "Not Started"  # Not Started, In Progress, Complete
        self.progress = 0  # 0-100
        self.notes = ""
        self.use_placeholder = not exists
        self.placeholder_path = ""
        self.settings: Dict = {}
        self.created_date = datetime.now().isoformat()
        self.modified_date = datetime.now().isoformat()

    def to_dict(self) -> dict:
        return {
            'name': self.name,
            'path': self.path,
            'exists': self.exists,
            'tags': list(self.tags),
            'status': self.status,
            'progress': self.progress,
            'notes': self.notes,
            'use_placeholder': self.use_placeholder,
            'placeholder_path': self.placeholder_path,
            'settings': self.settings,
            'created_date': self.created_date,
            'modified_date': self.modified_date
        }

    @staticmethod
    def from_dict(data: dict) -> 'Asset':
        asset = Asset(data['name'], data['path'], data.get('exists', False))
        asset.tags = set(data.get('tags', []))
        asset.status = data.get('status', 'Not Started')
        asset.progress = data.get('progress', 0)
        asset.notes = data.get('notes', '')
        asset.use_placeholder = data.get('use_placeholder', not asset.exists)
        asset.placeholder_path = data.get('placeholder_path', '')
        asset.settings = data.get('settings', {})
        asset.created_date = data.get('created_date', datetime.now().isoformat())
        asset.modified_date = data.get('modified_date', datetime.now().isoformat())
        return asset


class Tag:
    """Represents an asset tag with associated settings"""

    def __init__(self, name: str):
        self.name = name
        self.color = "#7b5fb8"
        self.placeholder_path = ""
        self.import_settings: Dict = {}

    def to_dict(self) -> dict:
        return {
            'name': self.name,
            'color': self.color,
            'placeholder_path': self.placeholder_path,
            'import_settings': self.import_settings
        }

    @staticmethod
    def from_dict(data: dict) -> 'Tag':
        tag = Tag(data['name'])
        tag.color = data.get('color', '#7b5fb8')
        tag.placeholder_path = data.get('placeholder_path', '')
        tag.import_settings = data.get('import_settings', {})
        return tag


class Rule:
    """Represents a tagging rule based on path patterns"""

    def __init__(self, name: str, pattern: str):
        self.name = name
        self.pattern = pattern  # Regex pattern
        self.tags: List[str] = []
        self.enabled = True

    def matches(self, path: str) -> bool:
        """Check if path matches this rule"""
        if not self.enabled:
            return False
        try:
            return bool(re.search(self.pattern, path, re.IGNORECASE))
        except:
            return False

    def to_dict(self) -> dict:
        return {
            'name': self.name,
            'pattern': self.pattern,
            'tags': self.tags,
            'enabled': self.enabled
        }

    @staticmethod
    def from_dict(data: dict) -> 'Rule':
        rule = Rule(data['name'], data['pattern'])
        rule.tags = data.get('tags', [])
        rule.enabled = data.get('enabled', True)
        return rule


class AssetTemplate:
    """Represents a template for creating asset files"""

    def __init__(self, name: str, file_pattern: str):
        self.name = name
        self.file_pattern = file_pattern  # e.g., "{name}Profile.png"
        self.tags: List[str] = []
        self.default_settings: Dict = {}

    def generate_filename(self, instance_name: str) -> str:
        """Generate filename from pattern"""
        return self.file_pattern.replace("{name}", instance_name)

    def to_dict(self) -> dict:
        return {
            'name': self.name,
            'file_pattern': self.file_pattern,
            'tags': self.tags,
            'default_settings': self.default_settings
        }

    @staticmethod
    def from_dict(data: dict) -> 'AssetTemplate':
        template = AssetTemplate(data['name'], data['file_pattern'])
        template.tags = data.get('tags', [])
        template.default_settings = data.get('default_settings', {})
        return template


class AssetGroup:
    """Represents a group of related assets"""

    def __init__(self, name: str):
        self.name = name
        self.templates: List[AssetTemplate] = []
        self.instances: List[str] = []  # Instance names
        self.base_path = ""  # Base directory for assets

    def to_dict(self) -> dict:
        return {
            'name': self.name,
            'templates': [t.to_dict() for t in self.templates],
            'instances': self.instances,
            'base_path': self.base_path
        }

    @staticmethod
    def from_dict(data: dict) -> 'AssetGroup':
        group = AssetGroup(data['name'])
        group.templates = [AssetTemplate.from_dict(t) for t in data.get('templates', [])]
        group.instances = data.get('instances', [])
        group.base_path = data.get('base_path', '')
        return group


class NewAssetDialog(QDialog):
    """Dialog for creating a new asset"""

    def __init__(self, parent=None, project_root=None, available_tags=None):
        super().__init__(parent)
        self.project_root = project_root
        self.available_tags = available_tags or []

        self.setWindowTitle("Create New Asset")
        self.setModal(True)
        self.setMinimumWidth(500)

        layout = QVBoxLayout()

        # Form layout
        form = QFormLayout()

        self.name_edit = QLineEdit()
        self.name_edit.setPlaceholderText("Asset name")
        form.addRow("Name:", self.name_edit)

        # Path selection
        path_layout = QHBoxLayout()
        self.path_edit = QLineEdit()
        self.path_edit.setPlaceholderText("Relative path from project root")
        path_layout.addWidget(self.path_edit)

        browse_btn = QPushButton("Browse...")
        browse_btn.clicked.connect(self._browse_path)
        browse_btn.setFixedWidth(80)
        path_layout.addWidget(browse_btn)

        form.addRow("Path:", path_layout)

        # Tags
        self.tags_edit = QLineEdit()
        self.tags_edit.setPlaceholderText("Comma-separated tags")
        form.addRow("Tags:", self.tags_edit)

        # Status
        self.status_combo = QComboBox()
        self.status_combo.addItems(["Not Started", "In Progress", "Complete"])
        form.addRow("Status:", self.status_combo)

        layout.addLayout(form)

        # Notes
        notes_label = QLabel("Notes:")
        layout.addWidget(notes_label)

        self.notes_edit = QTextEdit()
        self.notes_edit.setMaximumHeight(100)
        layout.addWidget(self.notes_edit)

        # Buttons
        buttons = QDialogButtonBox(
            QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel
        )
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)

        self.setLayout(layout)

    def _browse_path(self):
        """Browse for asset path"""
        if self.project_root:
            file_path, _ = QFileDialog.getSaveFileName(
                self,
                "Select Asset Location",
                str(self.project_root),
                "All Files (*.*)"
            )

            if file_path:
                # Make relative to project root
                try:
                    rel_path = Path(file_path).relative_to(self.project_root)
                    self.path_edit.setText(str(rel_path).replace('/', '\\'))
                except:
                    self.path_edit.setText(file_path)

    def get_asset_info(self) -> dict:
        """Get asset creation info"""
        return {
            'name': self.name_edit.text(),
            'path': self.path_edit.text(),
            'tags': [t.strip() for t in self.tags_edit.text().split(',') if t.strip()],
            'status': self.status_combo.currentText(),
            'notes': self.notes_edit.toPlainText()
        }


class RuleDialog(QDialog):
    """Dialog for creating/editing a rule"""

    def __init__(self, parent=None, rule=None, available_tags=None):
        super().__init__(parent)
        self.rule = rule
        self.available_tags = available_tags or []

        self.setWindowTitle("Edit Rule" if rule else "Create Rule")
        self.setModal(True)
        self.setMinimumWidth(500)

        layout = QVBoxLayout()

        # Form layout
        form = QFormLayout()

        self.name_edit = QLineEdit()
        self.name_edit.setPlaceholderText("Rule name")
        if rule:
            self.name_edit.setText(rule.name)
        form.addRow("Name:", self.name_edit)

        self.pattern_edit = QLineEdit()
        self.pattern_edit.setPlaceholderText("Regex pattern (e.g., assets\\\\icons\\\\.*)")
        if rule:
            self.pattern_edit.setText(rule.pattern)
        form.addRow("Pattern:", self.pattern_edit)

        self.tags_edit = QLineEdit()
        self.tags_edit.setPlaceholderText("Comma-separated tags to apply")
        if rule:
            self.tags_edit.setText(", ".join(rule.tags))
        form.addRow("Tags:", self.tags_edit)

        self.enabled_check = QCheckBox("Enabled")
        self.enabled_check.setChecked(rule.enabled if rule else True)
        form.addRow("", self.enabled_check)

        layout.addLayout(form)

        # Pattern help
        help_text = QLabel(
            "Pattern examples:\n"
            "- assets\\\\icons\\\\.* - Matches any file in assets\\icons\\\n"
            "- .*\\\\.png$ - Matches any PNG file\n"
            "- assets\\\\monsters\\\\.*\\\\icons\\\\.* - Matches icons in monster folders"
        )
        help_text.setProperty("secondary", True)
        help_text.setWordWrap(True)
        layout.addWidget(help_text)

        # Buttons
        buttons = QDialogButtonBox(
            QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel
        )
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)

        self.setLayout(layout)

    def get_rule_info(self) -> dict:
        """Get rule info"""
        return {
            'name': self.name_edit.text(),
            'pattern': self.pattern_edit.text(),
            'tags': [t.strip() for t in self.tags_edit.text().split(',') if t.strip()],
            'enabled': self.enabled_check.isChecked()
        }


class GroupDialog(QDialog):
    """Dialog for creating/editing an asset group"""

    def __init__(self, parent=None, group=None, project_root=None):
        super().__init__(parent)
        self.group = group
        self.project_root = project_root

        self.setWindowTitle("Edit Group" if group else "Create Asset Group")
        self.setModal(True)
        self.setMinimumWidth(600)
        self.setMinimumHeight(400)

        layout = QVBoxLayout()

        # Basic info
        form = QFormLayout()

        self.name_edit = QLineEdit()
        self.name_edit.setPlaceholderText("Group name (e.g., Monsters)")
        if group:
            self.name_edit.setText(group.name)
        form.addRow("Name:", self.name_edit)

        # Base path
        path_layout = QHBoxLayout()
        self.base_path_edit = QLineEdit()
        self.base_path_edit.setPlaceholderText("Base directory for assets")
        if group:
            self.base_path_edit.setText(group.base_path)
        path_layout.addWidget(self.base_path_edit)

        browse_btn = QPushButton("Browse...")
        browse_btn.clicked.connect(self._browse_base_path)
        browse_btn.setFixedWidth(80)
        path_layout.addWidget(browse_btn)

        form.addRow("Base Path:", path_layout)

        layout.addLayout(form)

        # Templates
        templates_label = QLabel("Asset Templates:")
        layout.addWidget(templates_label)

        # Template list and editor
        templates_splitter = QSplitter(Qt.Orientation.Horizontal)

        # Template list
        list_widget = QWidget()
        list_layout = QVBoxLayout()
        list_layout.setContentsMargins(0, 0, 0, 0)

        self.template_list = QListWidget()
        if group:
            for template in group.templates:
                self.template_list.addItem(f"{template.name}: {template.file_pattern}")
        self.template_list.currentRowChanged.connect(self._on_template_selected)
        list_layout.addWidget(self.template_list)

        # Template buttons
        template_btns = QHBoxLayout()

        add_template_btn = QPushButton("Add")
        add_template_btn.clicked.connect(self._add_template)
        template_btns.addWidget(add_template_btn)

        remove_template_btn = QPushButton("Remove")
        remove_template_btn.clicked.connect(self._remove_template)
        template_btns.addWidget(remove_template_btn)

        list_layout.addLayout(template_btns)

        list_widget.setLayout(list_layout)
        templates_splitter.addWidget(list_widget)

        # Template editor
        editor_widget = QWidget()
        editor_layout = QFormLayout()

        self.template_name_edit = QLineEdit()
        self.template_name_edit.setPlaceholderText("Template name")
        editor_layout.addRow("Name:", self.template_name_edit)

        self.template_pattern_edit = QLineEdit()
        self.template_pattern_edit.setPlaceholderText("{name}Profile.png")
        editor_layout.addRow("File Pattern:", self.template_pattern_edit)

        pattern_help = QLabel("Use {name} as placeholder for instance name")
        pattern_help.setProperty("secondary", True)
        editor_layout.addRow("", pattern_help)

        self.template_tags_edit = QLineEdit()
        self.template_tags_edit.setPlaceholderText("Comma-separated tags")
        editor_layout.addRow("Tags:", self.template_tags_edit)

        update_template_btn = QPushButton("Update Template")
        update_template_btn.clicked.connect(self._update_template)
        editor_layout.addRow("", update_template_btn)

        editor_widget.setLayout(editor_layout)
        templates_splitter.addWidget(editor_widget)

        layout.addWidget(templates_splitter)

        # Buttons
        buttons = QDialogButtonBox(
            QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel
        )
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)

        self.setLayout(layout)

        # Initialize templates
        self.templates = []
        if group:
            self.templates = [AssetTemplate(t.name, t.file_pattern) for t in group.templates]
            for i, template in enumerate(self.templates):
                template.tags = group.templates[i].tags.copy()

    def _browse_base_path(self):
        """Browse for base path"""
        if self.project_root:
            directory = QFileDialog.getExistingDirectory(
                self,
                "Select Base Directory",
                str(self.project_root)
            )

            if directory:
                try:
                    rel_path = Path(directory).relative_to(self.project_root)
                    self.base_path_edit.setText(str(rel_path).replace('/', '\\'))
                except:
                    self.base_path_edit.setText(directory)

    def _add_template(self):
        """Add new template"""
        template = AssetTemplate("New Template", "{name}.png")
        self.templates.append(template)
        self.template_list.addItem(f"{template.name}: {template.file_pattern}")
        self.template_list.setCurrentRow(len(self.templates) - 1)

    def _remove_template(self):
        """Remove selected template"""
        current_row = self.template_list.currentRow()
        if current_row >= 0:
            self.templates.pop(current_row)
            self.template_list.takeItem(current_row)

    def _on_template_selected(self, index):
        """Handle template selection"""
        if 0 <= index < len(self.templates):
            template = self.templates[index]
            self.template_name_edit.setText(template.name)
            self.template_pattern_edit.setText(template.file_pattern)
            self.template_tags_edit.setText(", ".join(template.tags))

    def _update_template(self):
        """Update current template"""
        current_row = self.template_list.currentRow()
        if 0 <= current_row < len(self.templates):
            template = self.templates[current_row]
            template.name = self.template_name_edit.text()
            template.file_pattern = self.template_pattern_edit.text()
            template.tags = [t.strip() for t in self.template_tags_edit.text().split(',') if t.strip()]

            # Update list item
            self.template_list.item(current_row).setText(f"{template.name}: {template.file_pattern}")

    def get_group_info(self) -> dict:
        """Get group info"""
        return {
            'name': self.name_edit.text(),
            'base_path': self.base_path_edit.text(),
            'templates': self.templates
        }


class AddInstanceDialog(QDialog):
    """Dialog for adding instances to a group"""

    def __init__(self, parent=None, group=None):
        super().__init__(parent)
        self.group = group

        self.setWindowTitle(f"Add Instance to {group.name if group else 'Group'}")
        self.setModal(True)
        self.setMinimumWidth(400)

        layout = QVBoxLayout()

        # Instance name
        form = QFormLayout()

        self.name_edit = QLineEdit()
        self.name_edit.setPlaceholderText("Instance name (e.g., Pikachu)")
        self.name_edit.textChanged.connect(self._update_preview)
        form.addRow("Name:", self.name_edit)

        layout.addLayout(form)

        # Preview
        preview_label = QLabel("Files that will be created:")
        layout.addWidget(preview_label)

        self.preview_list = QListWidget()
        layout.addWidget(self.preview_list)

        # Buttons
        buttons = QDialogButtonBox(
            QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel
        )
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)

        self.setLayout(layout)

    def _update_preview(self):
        """Update file preview"""
        self.preview_list.clear()

        if self.group and self.name_edit.text():
            name = self.name_edit.text()
            for template in self.group.templates:
                filename = template.generate_filename(name)
                full_path = f"{self.group.base_path}\\{filename}" if self.group.base_path else filename
                self.preview_list.addItem(full_path)

    def get_instance_name(self) -> str:
        """Get instance name"""
        return self.name_edit.text()


class AssetManager(EditorPanel):
    """Asset Manager tool for tracking and managing project assets"""

    def __init__(self, parent=None, project_root=None):
        self.project_root = Path(project_root) if project_root else None

        # Data structures
        self.assets: Dict[str, Asset] = {}  # path -> Asset
        self.tags: Dict[str, Tag] = {}  # name -> Tag
        self.rules: List[Rule] = []
        self.groups: Dict[str, AssetGroup] = {}  # name -> AssetGroup

        # File watcher
        self.file_watcher = None
        self.scan_timer = None

        super().__init__("Asset Manager", parent)
        self.setObjectName("AssetManager")

        # Load data if project root is set
        if self.project_root:
            self._load_data()
            self._scan_assets()
            self._setup_file_watcher()

    def _setup_panel(self):
        """Setup asset manager panel UI"""
        # Main splitter
        main_splitter = QSplitter(Qt.Orientation.Horizontal)

        # Left side - Asset tree
        left_widget = QWidget()
        left_layout = QVBoxLayout()
        left_layout.setContentsMargins(0, 0, 0, 0)

        # Toolbar
        toolbar = QHBoxLayout()
        toolbar.setContentsMargins(5, 5, 5, 5)

        scan_btn = QPushButton("Scan Assets")
        scan_btn.clicked.connect(self._scan_assets)
        toolbar.addWidget(scan_btn)

        new_asset_btn = QPushButton("New Asset")
        new_asset_btn.clicked.connect(self._create_new_asset)
        toolbar.addWidget(new_asset_btn)

        refresh_btn = QPushButton("Refresh")
        refresh_btn.clicked.connect(self._refresh_view)
        toolbar.addWidget(refresh_btn)

        toolbar.addStretch()

        left_layout.addLayout(toolbar)

        # Search bar
        search_layout = QHBoxLayout()
        search_layout.setContentsMargins(5, 0, 5, 5)

        self.search_edit = QLineEdit()
        self.search_edit.setPlaceholderText("Search assets...")
        self.search_edit.textChanged.connect(self._filter_assets)
        search_layout.addWidget(self.search_edit)

        self.filter_combo = QComboBox()
        self.filter_combo.addItems(["All", "Existing", "Missing", "In Progress", "Complete"])
        self.filter_combo.currentTextChanged.connect(self._filter_assets)
        search_layout.addWidget(self.filter_combo)

        left_layout.addLayout(search_layout)

        # Asset tree
        self.asset_tree = QTreeWidget()
        self.asset_tree.setHeaderLabels(["Name", "Status", "Progress", "Tags"])
        self.asset_tree.setColumnWidth(0, 250)
        self.asset_tree.setColumnWidth(1, 100)
        self.asset_tree.setColumnWidth(2, 80)
        self.asset_tree.setIconSize(QSize(32, 32))
        self.asset_tree.setUniformRowHeights(False)
        self.asset_tree.itemSelectionChanged.connect(self._on_asset_selected)
        self.asset_tree.setContextMenuPolicy(Qt.ContextMenuPolicy.CustomContextMenu)
        self.asset_tree.customContextMenuRequested.connect(self._show_asset_context_menu)
        left_layout.addWidget(self.asset_tree)

        # Stats bar
        self.stats_label = QLabel("Assets: 0 | Missing: 0 | Complete: 0")
        self.stats_label.setProperty("secondary", True)
        left_layout.addWidget(self.stats_label)

        left_widget.setLayout(left_layout)
        main_splitter.addWidget(left_widget)

        # Right side - Details and management tabs
        self.detail_tabs = QTabWidget()

        # Asset Details Tab
        self.detail_tabs.addTab(self._create_details_tab(), "Details")

        # Tag Management Tab
        self.detail_tabs.addTab(self._create_tags_tab(), "Tags")

        # Rules Tab
        self.detail_tabs.addTab(self._create_rules_tab(), "Rules")

        # Groups Tab
        self.detail_tabs.addTab(self._create_groups_tab(), "Groups")

        main_splitter.addWidget(self.detail_tabs)

        # Set splitter sizes
        main_splitter.setSizes([400, 600])

        self.content_layout.addWidget(main_splitter)

        # Initial refresh
        self._refresh_view()

    def _create_details_tab(self) -> QWidget:
        """Create asset details tab"""
        widget = QWidget()
        layout = QVBoxLayout()

        # Scroll area for details
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(QFrame.Shape.NoFrame)

        details_widget = QWidget()
        self.details_layout = QVBoxLayout()

        # Preview section
        preview_group = QGroupBox("Preview")
        preview_layout = QVBoxLayout()
        preview_layout.setAlignment(Qt.AlignmentFlag.AlignCenter)

        self.detail_preview_label = QLabel()
        self.detail_preview_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.detail_preview_label.setMinimumSize(200, 200)
        self.detail_preview_label.setMaximumSize(400, 400)
        self.detail_preview_label.setStyleSheet("QLabel { border: 1px solid #3d3650; background-color: #1e1a28; }")
        self.detail_preview_label.setScaledContents(False)
        preview_layout.addWidget(self.detail_preview_label)

        preview_group.setLayout(preview_layout)
        self.details_layout.addWidget(preview_group)

        # Asset info group
        info_group = QGroupBox("Asset Information")
        info_layout = QFormLayout()

        self.detail_name_label = QLabel("-")
        info_layout.addRow("Name:", self.detail_name_label)

        self.detail_path_label = QLabel("-")
        self.detail_path_label.setWordWrap(True)
        info_layout.addRow("Path:", self.detail_path_label)

        self.detail_exists_label = QLabel("-")
        info_layout.addRow("Exists:", self.detail_exists_label)

        self.detail_status_combo = QComboBox()
        self.detail_status_combo.addItems(["Not Started", "In Progress", "Complete"])
        self.detail_status_combo.currentTextChanged.connect(self._update_asset_status)
        info_layout.addRow("Status:", self.detail_status_combo)

        progress_layout = QHBoxLayout()
        self.detail_progress_spin = QSpinBox()
        self.detail_progress_spin.setRange(0, 100)
        self.detail_progress_spin.setSuffix("%")
        self.detail_progress_spin.valueChanged.connect(self._update_asset_progress)
        progress_layout.addWidget(self.detail_progress_spin)

        self.detail_progress_bar = QProgressBar()
        self.detail_progress_bar.setRange(0, 100)
        progress_layout.addWidget(self.detail_progress_bar)

        info_layout.addRow("Progress:", progress_layout)

        self.detail_tags_edit = QLineEdit()
        self.detail_tags_edit.editingFinished.connect(self._update_asset_tags)
        info_layout.addRow("Tags:", self.detail_tags_edit)

        info_group.setLayout(info_layout)
        self.details_layout.addWidget(info_group)

        # Placeholder group
        placeholder_group = QGroupBox("Placeholder Settings")
        placeholder_layout = QVBoxLayout()

        self.detail_use_placeholder_check = QCheckBox("Use placeholder for missing asset")
        self.detail_use_placeholder_check.stateChanged.connect(self._update_asset_placeholder_enabled)
        placeholder_layout.addWidget(self.detail_use_placeholder_check)

        placeholder_path_layout = QHBoxLayout()
        self.detail_placeholder_edit = QLineEdit()
        self.detail_placeholder_edit.setPlaceholderText("Placeholder asset path")
        self.detail_placeholder_edit.editingFinished.connect(self._update_asset_placeholder_path)
        placeholder_path_layout.addWidget(self.detail_placeholder_edit)

        browse_placeholder_btn = QPushButton("Browse...")
        browse_placeholder_btn.clicked.connect(self._browse_placeholder)
        browse_placeholder_btn.setFixedWidth(80)
        placeholder_path_layout.addWidget(browse_placeholder_btn)

        placeholder_layout.addLayout(placeholder_path_layout)

        placeholder_group.setLayout(placeholder_layout)
        self.details_layout.addWidget(placeholder_group)

        # Notes group
        notes_group = QGroupBox("Notes")
        notes_layout = QVBoxLayout()

        self.detail_notes_edit = QTextEdit()
        self.detail_notes_edit.setMaximumHeight(150)
        self.detail_notes_edit.textChanged.connect(self._update_asset_notes)
        notes_layout.addWidget(self.detail_notes_edit)

        notes_group.setLayout(notes_layout)
        self.details_layout.addWidget(notes_group)

        # Action buttons
        actions_layout = QHBoxLayout()

        open_file_btn = QPushButton("Open File")
        open_file_btn.clicked.connect(self._open_asset_file)
        actions_layout.addWidget(open_file_btn)

        open_folder_btn = QPushButton("Open Folder")
        open_folder_btn.clicked.connect(self._open_asset_folder)
        actions_layout.addWidget(open_folder_btn)

        delete_btn = QPushButton("Delete Asset")
        delete_btn.setProperty("danger", True)
        delete_btn.clicked.connect(self._delete_asset)
        actions_layout.addWidget(delete_btn)

        actions_layout.addStretch()

        self.details_layout.addLayout(actions_layout)

        self.details_layout.addStretch()

        details_widget.setLayout(self.details_layout)
        scroll.setWidget(details_widget)

        layout.addWidget(scroll)
        widget.setLayout(layout)

        return widget

    def _create_tags_tab(self) -> QWidget:
        """Create tag management tab"""
        widget = QWidget()
        layout = QVBoxLayout()

        # Toolbar
        toolbar = QHBoxLayout()

        new_tag_btn = QPushButton("New Tag")
        new_tag_btn.clicked.connect(self._create_tag)
        toolbar.addWidget(new_tag_btn)

        delete_tag_btn = QPushButton("Delete Tag")
        delete_tag_btn.clicked.connect(self._delete_tag)
        toolbar.addWidget(delete_tag_btn)

        toolbar.addStretch()

        layout.addLayout(toolbar)

        # Splitter
        splitter = QSplitter(Qt.Orientation.Horizontal)

        # Tag list
        self.tag_list = QListWidget()
        self.tag_list.currentRowChanged.connect(self._on_tag_selected)
        splitter.addWidget(self.tag_list)

        # Tag details
        tag_details = QWidget()
        tag_details_layout = QVBoxLayout()

        tag_form = QFormLayout()

        self.tag_name_edit = QLineEdit()
        self.tag_name_edit.editingFinished.connect(self._update_tag)
        tag_form.addRow("Name:", self.tag_name_edit)

        self.tag_color_edit = QLineEdit()
        self.tag_color_edit.setPlaceholderText("#7b5fb8")
        self.tag_color_edit.editingFinished.connect(self._update_tag)
        tag_form.addRow("Color:", self.tag_color_edit)

        tag_details_layout.addLayout(tag_form)

        # Placeholder for tag
        tag_placeholder_group = QGroupBox("Default Placeholder")
        tag_placeholder_layout = QVBoxLayout()

        tag_placeholder_help = QLabel("All assets with this tag will use this placeholder when missing")
        tag_placeholder_help.setProperty("secondary", True)
        tag_placeholder_help.setWordWrap(True)
        tag_placeholder_layout.addWidget(tag_placeholder_help)

        tag_placeholder_path_layout = QHBoxLayout()
        self.tag_placeholder_edit = QLineEdit()
        self.tag_placeholder_edit.editingFinished.connect(self._update_tag)
        tag_placeholder_path_layout.addWidget(self.tag_placeholder_edit)

        browse_tag_placeholder_btn = QPushButton("Browse...")
        browse_tag_placeholder_btn.clicked.connect(self._browse_tag_placeholder)
        browse_tag_placeholder_btn.setFixedWidth(80)
        tag_placeholder_path_layout.addWidget(browse_tag_placeholder_btn)

        tag_placeholder_layout.addLayout(tag_placeholder_path_layout)

        tag_placeholder_group.setLayout(tag_placeholder_layout)
        tag_details_layout.addWidget(tag_placeholder_group)

        tag_details_layout.addStretch()

        tag_details.setLayout(tag_details_layout)
        splitter.addWidget(tag_details)

        layout.addWidget(splitter)

        widget.setLayout(layout)
        return widget

    def _create_rules_tab(self) -> QWidget:
        """Create rules management tab"""
        widget = QWidget()
        layout = QVBoxLayout()

        # Toolbar
        toolbar = QHBoxLayout()

        new_rule_btn = QPushButton("New Rule")
        new_rule_btn.clicked.connect(self._create_rule)
        toolbar.addWidget(new_rule_btn)

        edit_rule_btn = QPushButton("Edit Rule")
        edit_rule_btn.clicked.connect(self._edit_rule)
        toolbar.addWidget(edit_rule_btn)

        delete_rule_btn = QPushButton("Delete Rule")
        delete_rule_btn.clicked.connect(self._delete_rule)
        toolbar.addWidget(delete_rule_btn)

        apply_rules_btn = QPushButton("Apply All Rules")
        apply_rules_btn.clicked.connect(self._apply_all_rules)
        toolbar.addWidget(apply_rules_btn)

        toolbar.addStretch()

        layout.addLayout(toolbar)

        # Rules list
        self.rules_table = QTableWidget()
        self.rules_table.setColumnCount(4)
        self.rules_table.setHorizontalHeaderLabels(["Enabled", "Name", "Pattern", "Tags"])
        self.rules_table.horizontalHeader().setSectionResizeMode(2, QHeaderView.ResizeMode.Stretch)
        self.rules_table.itemChanged.connect(self._on_rule_item_changed)
        layout.addWidget(self.rules_table)

        # Help text
        help_label = QLabel(
            "Rules automatically tag assets based on their path. "
            "Rules are applied in order from top to bottom."
        )
        help_label.setProperty("secondary", True)
        help_label.setWordWrap(True)
        layout.addWidget(help_label)

        widget.setLayout(layout)
        return widget

    def _create_groups_tab(self) -> QWidget:
        """Create groups management tab"""
        widget = QWidget()
        layout = QVBoxLayout()

        # Toolbar
        toolbar = QHBoxLayout()

        new_group_btn = QPushButton("New Group")
        new_group_btn.clicked.connect(self._create_group)
        toolbar.addWidget(new_group_btn)

        edit_group_btn = QPushButton("Edit Group")
        edit_group_btn.clicked.connect(self._edit_group)
        toolbar.addWidget(edit_group_btn)

        delete_group_btn = QPushButton("Delete Group")
        delete_group_btn.clicked.connect(self._delete_group)
        toolbar.addWidget(delete_group_btn)

        toolbar.addStretch()

        layout.addLayout(toolbar)

        # Splitter
        splitter = QSplitter(Qt.Orientation.Horizontal)

        # Group list
        self.group_list = QListWidget()
        self.group_list.currentRowChanged.connect(self._on_group_selected)
        splitter.addWidget(self.group_list)

        # Group details and instances
        group_details_widget = QWidget()
        group_details_layout = QVBoxLayout()

        # Group info
        self.group_info_label = QLabel("Select a group")
        self.group_info_label.setProperty("secondary", True)
        group_details_layout.addWidget(self.group_info_label)

        # Instances
        instances_label = QLabel("Instances:")
        group_details_layout.addWidget(instances_label)

        # Instance toolbar
        instance_toolbar = QHBoxLayout()

        add_instance_btn = QPushButton("Add Instance")
        add_instance_btn.clicked.connect(self._add_instance)
        instance_toolbar.addWidget(add_instance_btn)

        remove_instance_btn = QPushButton("Remove Instance")
        remove_instance_btn.clicked.connect(self._remove_instance)
        instance_toolbar.addWidget(remove_instance_btn)

        instance_toolbar.addStretch()

        group_details_layout.addLayout(instance_toolbar)

        self.instance_list = QListWidget()
        group_details_layout.addWidget(self.instance_list)

        group_details_widget.setLayout(group_details_layout)
        splitter.addWidget(group_details_widget)

        layout.addWidget(splitter)

        widget.setLayout(layout)
        return widget

    def _scan_assets(self):
        """Scan project for assets"""
        if not self.project_root or not self.project_root.exists():
            QMessageBox.warning(self, "Error", "Project root not set or doesn't exist")
            return

        # Scan for common asset types
        asset_extensions = [
            '.png', '.jpg', '.jpeg', '.bmp', '.gif', '.tga',  # Images
            '.ttf', '.otf',  # Fonts
            '.wav', '.mp3', '.ogg',  # Audio
            '.obj', '.fbx', '.gltf', '.glb',  # 3D Models
            '.lua', '.py', '.rb',  # Scripts
            '.json', '.xml', '.yaml',  # Data
            '.mov', '.mp4', '.avi'  # Video
        ]

        found_assets = []

        for ext in asset_extensions:
            for file_path in self.project_root.rglob(f'*{ext}'):
                try:
                    rel_path = str(file_path.relative_to(self.project_root)).replace('/', '\\')
                    found_assets.append(rel_path)
                except:
                    continue

        # Add new assets
        new_count = 0
        for rel_path in found_assets:
            if rel_path not in self.assets:
                asset = Asset(Path(rel_path).name, rel_path, True)
                self.assets[rel_path] = asset
                self._apply_rules_to_asset(asset)
                new_count += 1

        # Update existing assets
        for path, asset in self.assets.items():
            full_path = self.project_root / path
            asset.exists = full_path.exists()

        self._save_data()
        self._refresh_view()

        # Update stats label silently instead of showing popup
        self.stats_label.setText(
            f"Scan complete: {len(found_assets)} total, {new_count} new | "
            f"Assets: {len(self.assets)} | Missing: {sum(1 for a in self.assets.values() if not a.exists)} | "
            f"Complete: {sum(1 for a in self.assets.values() if a.status == 'Complete')}"
        )

    def _setup_file_watcher(self):
        """Setup file system watcher for automatic asset detection"""
        if not self.project_root or not self.project_root.exists():
            return

        self.file_watcher = QFileSystemWatcher()

        # Watch project root and all subdirectories
        dirs_to_watch = [str(self.project_root)]
        for subdir in self.project_root.rglob('*'):
            if subdir.is_dir():
                # Skip common directories that don't need watching
                skip_dirs = {'__pycache__', '.git', 'build', 'node_modules', '.vscode'}
                if subdir.name not in skip_dirs:
                    dirs_to_watch.append(str(subdir))

        self.file_watcher.addPaths(dirs_to_watch)
        self.file_watcher.directoryChanged.connect(self._on_directory_changed)

        # Setup debounce timer for scans
        self.scan_timer = QTimer()
        self.scan_timer.setSingleShot(True)
        self.scan_timer.timeout.connect(self._delayed_scan)

    def _on_directory_changed(self, path):
        """Handle directory change event"""
        # Debounce - only scan after 2 seconds of no changes
        if self.scan_timer:
            self.scan_timer.stop()
            self.scan_timer.start(2000)

    def _delayed_scan(self):
        """Perform a delayed scan after file changes"""
        # Re-add any new directories to the watcher
        if self.file_watcher and self.project_root:
            for subdir in self.project_root.rglob('*'):
                if subdir.is_dir():
                    skip_dirs = {'__pycache__', '.git', 'build', 'node_modules', '.vscode'}
                    if subdir.name not in skip_dirs:
                        dir_path = str(subdir)
                        if dir_path not in self.file_watcher.directories():
                            self.file_watcher.addPath(dir_path)

        self._scan_assets()

    def _apply_rules_to_asset(self, asset: Asset):
        """Apply tagging rules to an asset"""
        for rule in self.rules:
            if rule.matches(asset.path):
                asset.tags.update(rule.tags)

    def _apply_all_rules(self):
        """Apply all rules to all assets"""
        for asset in self.assets.values():
            self._apply_rules_to_asset(asset)

        self._save_data()
        self._refresh_view()

        QMessageBox.information(self, "Rules Applied", "All rules have been applied to assets")

    def _refresh_view(self):
        """Refresh the asset tree view"""
        self.asset_tree.clear()

        # Apply filters
        search_text = self.search_edit.text().lower()
        filter_type = self.filter_combo.currentText()

        # Count stats
        total = len(self.assets)
        missing = sum(1 for a in self.assets.values() if not a.exists)
        complete = sum(1 for a in self.assets.values() if a.status == "Complete")

        # Build tree
        for path, asset in sorted(self.assets.items()):
            # Apply filters
            if search_text and search_text not in asset.name.lower() and search_text not in path.lower():
                continue

            if filter_type == "Existing" and not asset.exists:
                continue
            elif filter_type == "Missing" and asset.exists:
                continue
            elif filter_type == "In Progress" and asset.status != "In Progress":
                continue
            elif filter_type == "Complete" and asset.status != "Complete":
                continue

            # Create tree item
            item = QTreeWidgetItem([
                asset.name,
                asset.status,
                f"{asset.progress}%",
                ", ".join(sorted(asset.tags))
            ])

            # Generate and set icon
            if asset.exists and self.project_root:
                full_path = self.project_root / path
                # Check if it's an image file
                image_extensions = {'.png', '.jpg', '.jpeg', '.bmp', '.gif', '.tga'}
                if full_path.suffix.lower() in image_extensions:
                    icon = create_thumbnail(full_path, 32)
                else:
                    icon = create_generic_icon(full_path.suffix, 32)
                item.setIcon(0, icon)
            else:
                # Missing file - use generic icon
                icon = create_generic_icon(Path(path).suffix, 32)
                item.setIcon(0, icon)

            # Color based on status
            if not asset.exists:
                item.setForeground(0, QBrush(QColor("#d32f2f")))
            elif asset.status == "Complete":
                item.setForeground(0, QBrush(QColor("#4caf50")))
            elif asset.status == "In Progress":
                item.setForeground(0, QBrush(QColor("#f57c00")))

            item.setData(0, Qt.ItemDataRole.UserRole, path)
            self.asset_tree.addTopLevelItem(item)

        # Update stats
        self.stats_label.setText(f"Assets: {total} | Missing: {missing} | Complete: {complete}")

    def _filter_assets(self):
        """Filter assets based on search and filter"""
        self._refresh_view()

    def _on_asset_selected(self):
        """Handle asset selection"""
        items = self.asset_tree.selectedItems()
        if not items:
            return

        path = items[0].data(0, Qt.ItemDataRole.UserRole)
        asset = self.assets.get(path)

        if asset:
            self._update_details_panel(asset)

    def _update_details_panel(self, asset: Asset):
        """Update the details panel with asset info"""
        self.detail_name_label.setText(asset.name)
        self.detail_path_label.setText(asset.path)
        self.detail_exists_label.setText("Yes" if asset.exists else "No")
        self.detail_status_combo.setCurrentText(asset.status)
        self.detail_progress_spin.setValue(asset.progress)
        self.detail_progress_bar.setValue(asset.progress)
        self.detail_tags_edit.setText(", ".join(sorted(asset.tags)))
        self.detail_use_placeholder_check.setChecked(asset.use_placeholder)
        self.detail_placeholder_edit.setText(asset.placeholder_path)
        self.detail_notes_edit.setPlainText(asset.notes)

        # Update preview
        if asset.exists and self.project_root:
            full_path = self.project_root / asset.path
            # Check if it's an image file
            image_extensions = {'.png', '.jpg', '.jpeg', '.bmp', '.gif', '.tga'}
            if full_path.suffix.lower() in image_extensions:
                try:
                    pixmap = QPixmap(str(full_path))
                    if not pixmap.isNull():
                        # Scale to fit preview area while maintaining aspect ratio
                        scaled = pixmap.scaled(
                            380, 380,
                            Qt.AspectRatioMode.KeepAspectRatio,
                            Qt.TransformationMode.SmoothTransformation
                        )
                        self.detail_preview_label.setPixmap(scaled)
                    else:
                        self.detail_preview_label.setText("Cannot load image")
                except:
                    self.detail_preview_label.setText("Error loading image")
            else:
                # Show file type icon for non-images
                icon = create_generic_icon(full_path.suffix, 128)
                self.detail_preview_label.setPixmap(icon.pixmap(128, 128))
        else:
            # Missing file
            self.detail_preview_label.setText("Asset not found")

    def _get_selected_asset(self) -> Optional[Asset]:
        """Get currently selected asset"""
        items = self.asset_tree.selectedItems()
        if items:
            path = items[0].data(0, Qt.ItemDataRole.UserRole)
            return self.assets.get(path)
        return None

    def _update_asset_status(self, status: str):
        """Update asset status"""
        asset = self._get_selected_asset()
        if asset:
            asset.status = status
            asset.modified_date = datetime.now().isoformat()
            self._save_data()
            self._refresh_view()

    def _update_asset_progress(self, progress: int):
        """Update asset progress"""
        asset = self._get_selected_asset()
        if asset:
            asset.progress = progress
            asset.modified_date = datetime.now().isoformat()
            self.detail_progress_bar.setValue(progress)
            self._save_data()
            self._refresh_view()

    def _update_asset_tags(self):
        """Update asset tags"""
        asset = self._get_selected_asset()
        if asset:
            tags_text = self.detail_tags_edit.text()
            new_tags = set(t.strip() for t in tags_text.split(',') if t.strip())

            # Auto-create tags that don't exist yet
            for tag_name in new_tags:
                if tag_name and tag_name not in self.tags:
                    self.tags[tag_name] = Tag(tag_name)

            asset.tags = new_tags
            asset.modified_date = datetime.now().isoformat()
            self._save_data()
            self._refresh_view()
            self._refresh_tag_list()

    def _update_asset_placeholder_enabled(self, state):
        """Update asset placeholder enabled"""
        asset = self._get_selected_asset()
        if asset:
            asset.use_placeholder = bool(state)
            asset.modified_date = datetime.now().isoformat()
            self._save_data()

    def _update_asset_placeholder_path(self):
        """Update asset placeholder path"""
        asset = self._get_selected_asset()
        if asset:
            asset.placeholder_path = self.detail_placeholder_edit.text()
            asset.modified_date = datetime.now().isoformat()
            self._save_data()

    def _update_asset_notes(self):
        """Update asset notes"""
        asset = self._get_selected_asset()
        if asset:
            asset.notes = self.detail_notes_edit.toPlainText()
            asset.modified_date = datetime.now().isoformat()
            self._save_data()

    def _browse_placeholder(self):
        """Browse for placeholder asset"""
        if not self.project_root:
            return

        file_path, _ = QFileDialog.getOpenFileName(
            self,
            "Select Placeholder Asset",
            str(self.project_root),
            "All Files (*.*)"
        )

        if file_path:
            try:
                rel_path = Path(file_path).relative_to(self.project_root)
                self.detail_placeholder_edit.setText(str(rel_path).replace('/', '\\'))
                self._update_asset_placeholder_path()
            except:
                QMessageBox.warning(self, "Error", "Placeholder must be within project root")

    def _open_asset_file(self):
        """Open asset file"""
        asset = self._get_selected_asset()
        if not asset:
            QMessageBox.warning(self, "Error", "No asset selected")
            return

        if not self.project_root:
            QMessageBox.warning(self, "Error", "Project root not set")
            return

        full_path = self.project_root / asset.path
        if full_path.exists():
            try:
                import os
                os.startfile(str(full_path))
            except Exception as e:
                QMessageBox.warning(self, "Error", f"Failed to open file:\n{str(e)}")
        else:
            QMessageBox.warning(self, "Error", "File does not exist")

    def _open_asset_folder(self):
        """Open asset folder"""
        asset = self._get_selected_asset()
        if not asset:
            QMessageBox.warning(self, "Error", "No asset selected")
            return

        if not self.project_root:
            QMessageBox.warning(self, "Error", "Project root not set")
            return

        full_path = self.project_root / asset.path
        folder = full_path.parent
        if folder.exists():
            try:
                import os
                os.startfile(str(folder))
            except Exception as e:
                QMessageBox.warning(self, "Error", f"Failed to open folder:\n{str(e)}")
        else:
            QMessageBox.warning(self, "Error", "Folder does not exist")

    def _delete_asset(self):
        """Delete asset from tracking"""
        asset = self._get_selected_asset()
        if not asset:
            QMessageBox.warning(self, "Error", "No asset selected")
            return

        reply = QMessageBox.question(
            self,
            "Delete Asset",
            f"Remove '{asset.name}' from asset tracking?\n(File will not be deleted)",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
        )

        if reply == QMessageBox.StandardButton.Yes:
            del self.assets[asset.path]
            self._save_data()
            self._refresh_view()

    def _show_asset_context_menu(self, position):
        """Show context menu for asset"""
        item = self.asset_tree.itemAt(position)
        if not item:
            return

        menu = QMenu()

        open_action = QAction("Open File", self)
        open_action.triggered.connect(self._open_asset_file)
        menu.addAction(open_action)

        open_folder_action = QAction("Open Folder", self)
        open_folder_action.triggered.connect(self._open_asset_folder)
        menu.addAction(open_folder_action)

        menu.addSeparator()

        delete_action = QAction("Remove from Tracking", self)
        delete_action.triggered.connect(self._delete_asset)
        menu.addAction(delete_action)

        menu.exec(self.asset_tree.viewport().mapToGlobal(position))

    def _create_new_asset(self):
        """Create a new asset"""
        dialog = NewAssetDialog(self, self.project_root, list(self.tags.keys()))
        if dialog.exec() == QDialog.DialogCode.Accepted:
            info = dialog.get_asset_info()

            if not info['name'] or not info['path']:
                QMessageBox.warning(self, "Error", "Name and path are required")
                return

            # Check if already exists
            if info['path'] in self.assets:
                QMessageBox.warning(self, "Error", "Asset already exists")
                return

            # Create asset
            asset = Asset(info['name'], info['path'], False)
            asset.tags = set(info['tags'])
            asset.status = info['status']
            asset.notes = info['notes']

            self.assets[info['path']] = asset

            self._save_data()
            self._refresh_view()

    # Tag Management
    def _refresh_tag_list(self):
        """Refresh tag list"""
        self.tag_list.clear()
        for tag_name in sorted(self.tags.keys()):
            self.tag_list.addItem(tag_name)

    def _create_tag(self):
        """Create a new tag"""
        tag_name, ok = QInputDialog.getText(self, "New Tag", "Tag name:")
        if ok and tag_name:
            if tag_name in self.tags:
                QMessageBox.warning(self, "Error", "Tag already exists")
                return

            self.tags[tag_name] = Tag(tag_name)
            self._save_data()
            self._refresh_tag_list()

    def _delete_tag(self):
        """Delete selected tag"""
        current_row = self.tag_list.currentRow()
        if current_row >= 0:
            tag_name = self.tag_list.item(current_row).text()

            reply = QMessageBox.question(
                self,
                "Delete Tag",
                f"Delete tag '{tag_name}'?\n(Will be removed from all assets)",
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
            )

            if reply == QMessageBox.StandardButton.Yes:
                # Remove from all assets
                for asset in self.assets.values():
                    asset.tags.discard(tag_name)

                # Remove tag
                del self.tags[tag_name]

                self._save_data()
                self._refresh_tag_list()
                self._refresh_view()

    def _on_tag_selected(self, index):
        """Handle tag selection"""
        if index >= 0:
            tag_name = self.tag_list.item(index).text()
            tag = self.tags.get(tag_name)

            if tag:
                self.tag_name_edit.setText(tag.name)
                self.tag_color_edit.setText(tag.color)
                self.tag_placeholder_edit.setText(tag.placeholder_path)

    def _update_tag(self):
        """Update tag settings"""
        current_row = self.tag_list.currentRow()
        if current_row >= 0:
            old_name = self.tag_list.item(current_row).text()
            tag = self.tags.get(old_name)

            if tag:
                new_name = self.tag_name_edit.text()

                # Handle rename
                if new_name != old_name:
                    # Update in all assets
                    for asset in self.assets.values():
                        if old_name in asset.tags:
                            asset.tags.discard(old_name)
                            asset.tags.add(new_name)

                    # Update in tags dict
                    del self.tags[old_name]
                    tag.name = new_name
                    self.tags[new_name] = tag

                tag.color = self.tag_color_edit.text()
                tag.placeholder_path = self.tag_placeholder_edit.text()

                self._save_data()
                self._refresh_tag_list()
                self._refresh_view()

    def _browse_tag_placeholder(self):
        """Browse for tag placeholder"""
        if not self.project_root:
            return

        file_path, _ = QFileDialog.getOpenFileName(
            self,
            "Select Placeholder Asset",
            str(self.project_root),
            "All Files (*.*)"
        )

        if file_path:
            try:
                rel_path = Path(file_path).relative_to(self.project_root)
                self.tag_placeholder_edit.setText(str(rel_path).replace('/', '\\'))
                self._update_tag()
            except:
                QMessageBox.warning(self, "Error", "Placeholder must be within project root")

    # Rule Management
    def _refresh_rules_table(self):
        """Refresh rules table"""
        self.rules_table.setRowCount(0)

        for i, rule in enumerate(self.rules):
            self.rules_table.insertRow(i)

            # Enabled checkbox
            enabled_item = QTableWidgetItem()
            enabled_item.setCheckState(Qt.CheckState.Checked if rule.enabled else Qt.CheckState.Unchecked)
            self.rules_table.setItem(i, 0, enabled_item)

            # Name
            self.rules_table.setItem(i, 1, QTableWidgetItem(rule.name))

            # Pattern
            self.rules_table.setItem(i, 2, QTableWidgetItem(rule.pattern))

            # Tags
            self.rules_table.setItem(i, 3, QTableWidgetItem(", ".join(rule.tags)))

    def _create_rule(self):
        """Create a new rule"""
        dialog = RuleDialog(self, None, list(self.tags.keys()))
        if dialog.exec() == QDialog.DialogCode.Accepted:
            info = dialog.get_rule_info()

            if not info['name'] or not info['pattern']:
                QMessageBox.warning(self, "Error", "Name and pattern are required")
                return

            rule = Rule(info['name'], info['pattern'])
            rule.tags = info['tags']
            rule.enabled = info['enabled']

            self.rules.append(rule)

            self._save_data()
            self._refresh_rules_table()

    def _edit_rule(self):
        """Edit selected rule"""
        current_row = self.rules_table.currentRow()
        if current_row >= 0 and current_row < len(self.rules):
            rule = self.rules[current_row]

            dialog = RuleDialog(self, rule, list(self.tags.keys()))
            if dialog.exec() == QDialog.DialogCode.Accepted:
                info = dialog.get_rule_info()

                rule.name = info['name']
                rule.pattern = info['pattern']
                rule.tags = info['tags']
                rule.enabled = info['enabled']

                self._save_data()
                self._refresh_rules_table()

    def _delete_rule(self):
        """Delete selected rule"""
        current_row = self.rules_table.currentRow()
        if current_row >= 0 and current_row < len(self.rules):
            rule = self.rules[current_row]

            reply = QMessageBox.question(
                self,
                "Delete Rule",
                f"Delete rule '{rule.name}'?",
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
            )

            if reply == QMessageBox.StandardButton.Yes:
                self.rules.pop(current_row)
                self._save_data()
                self._refresh_rules_table()

    def _on_rule_item_changed(self, item):
        """Handle rule item change (for enabled checkbox)"""
        row = item.row()
        col = item.column()

        if col == 0 and row < len(self.rules):
            self.rules[row].enabled = item.checkState() == Qt.CheckState.Checked
            self._save_data()

    # Group Management
    def _refresh_group_list(self):
        """Refresh group list"""
        self.group_list.clear()
        for group_name in sorted(self.groups.keys()):
            self.group_list.addItem(group_name)

    def _create_group(self):
        """Create a new group"""
        dialog = GroupDialog(self, None, self.project_root)
        if dialog.exec() == QDialog.DialogCode.Accepted:
            info = dialog.get_group_info()

            if not info['name']:
                QMessageBox.warning(self, "Error", "Group name is required")
                return

            if info['name'] in self.groups:
                QMessageBox.warning(self, "Error", "Group already exists")
                return

            group = AssetGroup(info['name'])
            group.base_path = info['base_path']
            group.templates = info['templates']

            self.groups[info['name']] = group

            self._save_data()
            self._refresh_group_list()

    def _edit_group(self):
        """Edit selected group"""
        current_row = self.group_list.currentRow()
        if current_row >= 0:
            group_name = self.group_list.item(current_row).text()
            group = self.groups.get(group_name)

            if group:
                dialog = GroupDialog(self, group, self.project_root)
                if dialog.exec() == QDialog.DialogCode.Accepted:
                    info = dialog.get_group_info()

                    # Handle rename
                    if info['name'] != group_name:
                        del self.groups[group_name]
                        group.name = info['name']
                        self.groups[info['name']] = group

                    group.base_path = info['base_path']
                    group.templates = info['templates']

                    self._save_data()
                    self._refresh_group_list()

    def _delete_group(self):
        """Delete selected group"""
        current_row = self.group_list.currentRow()
        if current_row >= 0:
            group_name = self.group_list.item(current_row).text()

            reply = QMessageBox.question(
                self,
                "Delete Group",
                f"Delete group '{group_name}'?\n(Assets will not be deleted)",
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
            )

            if reply == QMessageBox.StandardButton.Yes:
                del self.groups[group_name]
                self._save_data()
                self._refresh_group_list()
                self._on_group_selected(-1)

    def _on_group_selected(self, index):
        """Handle group selection"""
        if index >= 0:
            group_name = self.group_list.item(index).text()
            group = self.groups.get(group_name)

            if group:
                # Update info
                info_text = f"<b>{group.name}</b><br>"
                info_text += f"Base Path: {group.base_path}<br>"
                info_text += f"Templates: {len(group.templates)}<br>"
                info_text += f"Instances: {len(group.instances)}"
                self.group_info_label.setText(info_text)

                # Update instances
                self.instance_list.clear()
                for instance in group.instances:
                    self.instance_list.addItem(instance)
        else:
            self.group_info_label.setText("Select a group")
            self.instance_list.clear()

    def _add_instance(self):
        """Add instance to group"""
        current_row = self.group_list.currentRow()
        if current_row >= 0:
            group_name = self.group_list.item(current_row).text()
            group = self.groups.get(group_name)

            if group:
                dialog = AddInstanceDialog(self, group)
                if dialog.exec() == QDialog.DialogCode.Accepted:
                    instance_name = dialog.get_instance_name()

                    if not instance_name:
                        return

                    # Add instance
                    group.instances.append(instance_name)

                    # Create assets from templates
                    for template in group.templates:
                        filename = template.generate_filename(instance_name)
                        rel_path = f"{group.base_path}\\{filename}" if group.base_path else filename

                        if rel_path not in self.assets:
                            asset = Asset(filename, rel_path, False)
                            asset.tags = set(template.tags)
                            asset.settings = template.default_settings.copy()
                            self.assets[rel_path] = asset

                    self._save_data()
                    self._on_group_selected(current_row)
                    self._refresh_view()

    def _remove_instance(self):
        """Remove instance from group"""
        current_row = self.group_list.currentRow()
        instance_row = self.instance_list.currentRow()

        if current_row >= 0 and instance_row >= 0:
            group_name = self.group_list.item(current_row).text()
            group = self.groups.get(group_name)

            if group and instance_row < len(group.instances):
                instance_name = group.instances[instance_row]

                reply = QMessageBox.question(
                    self,
                    "Remove Instance",
                    f"Remove instance '{instance_name}'?\n(Assets will remain in tracking)",
                    QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
                )

                if reply == QMessageBox.StandardButton.Yes:
                    group.instances.pop(instance_row)
                    self._save_data()
                    self._on_group_selected(current_row)

    # Data persistence
    def _get_data_file(self) -> Path:
        """Get asset manager data file path"""
        if self.project_root:
            data_dir = self.project_root / "production"
            data_dir.mkdir(exist_ok=True)
            return data_dir / "asset_manager.json"
        return None

    def _save_data(self):
        """Save asset manager data"""
        data_file = self._get_data_file()
        if not data_file:
            return

        data = {
            'assets': {path: asset.to_dict() for path, asset in self.assets.items()},
            'tags': {name: tag.to_dict() for name, tag in self.tags.items()},
            'rules': [rule.to_dict() for rule in self.rules],
            'groups': {name: group.to_dict() for name, group in self.groups.items()}
        }

        try:
            with open(data_file, 'w', encoding='utf-8') as f:
                json.dump(data, f, indent=2)
        except Exception as e:
            print(f"Failed to save asset manager data: {e}")

    def _load_data(self):
        """Load asset manager data"""
        data_file = self._get_data_file()
        if not data_file or not data_file.exists():
            return

        try:
            with open(data_file, 'r', encoding='utf-8') as f:
                data = json.load(f)

            # Load assets
            self.assets = {
                path: Asset.from_dict(asset_data)
                for path, asset_data in data.get('assets', {}).items()
            }

            # Load tags
            self.tags = {
                name: Tag.from_dict(tag_data)
                for name, tag_data in data.get('tags', {}).items()
            }

            # Load rules
            self.rules = [
                Rule.from_dict(rule_data)
                for rule_data in data.get('rules', [])
            ]

            # Load groups
            self.groups = {
                name: AssetGroup.from_dict(group_data)
                for name, group_data in data.get('groups', {}).items()
            }

            # Refresh UI
            self._refresh_tag_list()
            self._refresh_rules_table()
            self._refresh_group_list()

        except Exception as e:
            print(f"Failed to load asset manager data: {e}")

    def get_state(self) -> dict:
        """Get panel state"""
        state = super().get_state()
        state['current_tab'] = self.detail_tabs.currentIndex()
        return state

    def set_state(self, state: dict):
        """Restore panel state"""
        super().set_state(state)
        if 'current_tab' in state:
            self.detail_tabs.setCurrentIndex(state['current_tab'])
