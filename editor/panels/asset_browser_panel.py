"""
Asset Browser Panel
Browse and manage project assets
"""

from PyQt6.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QLineEdit, 
                             QPushButton, QListWidget, QComboBox, QLabel)
from PyQt6.QtCore import Qt, pyqtSignal
from .base_panel import EditorPanel


class AssetBrowserPanel(EditorPanel):
    """Asset browser panel for managing project assets"""
    
    # Signals
    asset_selected = pyqtSignal(str)  # Emits asset path
    asset_imported = pyqtSignal(str)  # Emits imported asset path
    
    def __init__(self, parent=None):
        super().__init__("Asset Browser", parent)
        self.setObjectName("AssetBrowserPanel")
    
    def _setup_panel(self):
        """Setup asset browser panel UI"""
        # Search and filter bar
        search_layout = QHBoxLayout()
        search_layout.setContentsMargins(5, 5, 5, 5)
        
        self.search_box = QLineEdit()
        self.search_box.setPlaceholderText("Search assets...")
        search_layout.addWidget(self.search_box)
        
        self.filter_combo = QComboBox()
        self.filter_combo.addItems(["All", "Scenes", "Scripts", "Textures", "Models", "Audio"])
        search_layout.addWidget(self.filter_combo)
        
        # Toolbar
        toolbar_layout = QHBoxLayout()
        toolbar_layout.setContentsMargins(5, 5, 5, 5)
        
        import_btn = QPushButton("Import")
        import_btn.setToolTip("Import assets into project")
        toolbar_layout.addWidget(import_btn)
        
        create_btn = QPushButton("Create")
        create_btn.setToolTip("Create new asset")
        toolbar_layout.addWidget(create_btn)
        
        toolbar_layout.addStretch()
        
        view_label = QLabel("View:")
        toolbar_layout.addWidget(view_label)
        
        self.view_combo = QComboBox()
        self.view_combo.addItems(["Grid", "List"])
        toolbar_layout.addWidget(self.view_combo)
        
        # Asset list/grid
        self.asset_list = QListWidget()
        self.asset_list.setViewMode(QListWidget.ViewMode.IconMode)
        self.asset_list.setResizeMode(QListWidget.ResizeMode.Adjust)
        self.asset_list.setSpacing(10)
        
        # Add to layout
        self.content_layout.addLayout(search_layout)
        self.content_layout.addLayout(toolbar_layout)
        self.content_layout.addWidget(self.asset_list)
