"""
Lupine Engine Project Manager
Main window for managing projects - create new, open existing, view recent projects
"""

from PyQt6.QtWidgets import (QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
                             QLabel, QPushButton, QListWidget, QListWidgetItem,
                             QFileDialog, QMessageBox, QFrame, QSizePolicy)
from PyQt6.QtCore import Qt, QSize, pyqtSignal
from PyQt6.QtGui import QPixmap, QIcon
from pathlib import Path
from datetime import datetime
import os

from project_file import ProjectFile, ProjectData
from dialogs import NewProjectDialog


ITEM_HEIGHT = 96
ICON_SIZE = 56


class ProjectListItem(QFrame):
    """Custom widget for displaying project information in the list"""

    clicked = pyqtSignal(str)       # Emits project path when activated
    remove_requested = pyqtSignal(str)  # Emits project path when removed from recents

    def __init__(self, project_path: str, project_name: str, last_opened: str, icon_path: str = None):
        super().__init__()
        self.project_path = project_path
        self.setObjectName("projectCard")

        layout = QHBoxLayout()
        layout.setContentsMargins(5, 5, 5, 5)
        layout.setSpacing(16)

        # Project icon
        icon_label = QLabel()
        if icon_path and os.path.exists(icon_path):
            pixmap = QPixmap(icon_path).scaled(ICON_SIZE, ICON_SIZE,
                                               Qt.AspectRatioMode.KeepAspectRatio,
                                               Qt.TransformationMode.SmoothTransformation)
            icon_label.setPixmap(pixmap)
        else:
            icon_label.setText("🎮")
            icon_label.setStyleSheet("font-size: 36px;")

        icon_label.setFixedSize(ICON_SIZE, ICON_SIZE)
        icon_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        layout.addWidget(icon_label, alignment=Qt.AlignmentFlag.AlignVCenter)

        # Project info
        info_layout = QVBoxLayout()
        info_layout.setSpacing(3)
        info_layout.setContentsMargins(0, 0, 0, 0)

        name_label = QLabel(project_name)
        name_label.setObjectName("projectName")
        name_label.setStyleSheet("font-size: 15px; font-weight: bold;")
        name_label.setWordWrap(False)
        name_label.setMinimumHeight(22)
        name_label.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Minimum)
        info_layout.addWidget(name_label)

        path_label = QLabel(project_path)
        path_label.setProperty("secondary", True)
        path_label.setStyleSheet("font-size: 11px;")
        path_label.setMinimumHeight(16)
        path_label.setWordWrap(False)
        info_layout.addWidget(path_label)

        # Format last opened date
        try:
            date_obj = datetime.fromisoformat(last_opened)
            date_str = date_obj.strftime("%B %d, %Y at %I:%M %p")
        except Exception:
            date_str = last_opened

        date_label = QLabel(f"Last opened: {date_str}")
        date_label.setProperty("secondary", True)
        date_label.setStyleSheet("font-size: 10px; font-style: italic;")
        date_label.setMinimumHeight(14)
        info_layout.addWidget(date_label)

        layout.addLayout(info_layout, stretch=1)

        # Remove-from-recents button
        self.remove_button = QPushButton("✕")
        self.remove_button.setObjectName("removeButton")
        self.remove_button.setFixedSize(26, 26)
        self.remove_button.setToolTip("Remove from recent projects")
        self.remove_button.setCursor(Qt.CursorShape.PointingHandCursor)
        self.remove_button.clicked.connect(self._on_remove)
        layout.addWidget(self.remove_button, alignment=Qt.AlignmentFlag.AlignTop)

        self.setLayout(layout)
        self.setCursor(Qt.CursorShape.PointingHandCursor)

    def _on_remove(self):
        """Handle the remove button click without triggering project open"""
        self.remove_requested.emit(self.project_path)

    def mousePressEvent(self, event):
        """Handle mouse click"""
        if event.button() == Qt.MouseButton.LeftButton:
            self.clicked.emit(self.project_path)
        super().mousePressEvent(event)


class ProjectManager(QMainWindow):
    """Main project manager window"""
    
    project_selected = pyqtSignal(object)  # Emits ProjectData when project is opened
    
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Lupine Engine - Project Manager")
        self.setMinimumSize(900, 600)
        
        self.recent_projects = []
        
        self._setup_ui()
        self._load_recent_projects()
    
    def _setup_ui(self):
        """Setup the project manager UI"""
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        
        main_layout = QHBoxLayout()
        main_layout.setSpacing(0)
        main_layout.setContentsMargins(0, 0, 0, 0)
        
        # Left panel - Logo and buttons
        left_panel = self._create_left_panel()
        main_layout.addWidget(left_panel)
        
        # Right panel - Recent projects
        right_panel = self._create_right_panel()
        main_layout.addWidget(right_panel, stretch=1)

        central_widget.setLayout(main_layout)
        self._apply_card_styles()

    def _apply_card_styles(self):
        """Apply themed styling to the recent-project cards"""
        try:
            from theme import get_theme_manager
            colors = get_theme_manager().current_theme.colors
            surface = colors.surface
            surface_hover = colors.surface_hover
            border = colors.border
            border_focus = colors.border_focus
            error = colors.error_color
            text = colors.text_primary
        except Exception:
            surface = "#141317"
            surface_hover = "#1e1b24"
            border = "#27242f"
            border_focus = "#514864"
            error = "#d32f2f"
            text = "#e8e6f0"

        self.setStyleSheet(self.styleSheet() + f"""
            QFrame#projectCard {{
                background-color: {surface};
                border: 1px solid {border};
                border-radius: 8px;
            }}
            QFrame#projectCard:hover {{
                background-color: {surface_hover};
                border: 1px solid {border_focus};
            }}
            QFrame#projectCard QLabel {{
                background: transparent;
                border: none;
            }}
            QPushButton#removeButton {{
                background-color: transparent;
                border: none;
                border-radius: 13px;
                color: {text};
                font-size: 13px;
                font-weight: bold;
            }}
            QPushButton#removeButton:hover {{
                background-color: {error};
                color: #ffffff;
            }}
        """)
    
    def _create_left_panel(self) -> QWidget:
        """Create the left panel with logo and action buttons"""
        panel = QWidget()
        panel.setFixedWidth(300)
        panel.setStyleSheet("background-color: rgba(0, 0, 0, 0.2);")
        
        layout = QVBoxLayout()
        layout.setContentsMargins(30, 30, 30, 30)
        layout.setSpacing(20)
        
        # Logo
        logo_label = QLabel()
        logo_path = Path(__file__).parent.parent / "logo.png"
        
        if logo_path.exists():
            pixmap = QPixmap(str(logo_path))
            scaled_pixmap = pixmap.scaled(200, 200, Qt.AspectRatioMode.KeepAspectRatio,
                                         Qt.TransformationMode.SmoothTransformation)
            logo_label.setPixmap(scaled_pixmap)
        else:
            logo_label.setText("🐺")
            logo_label.setStyleSheet("font-size: 80px;")
        
        logo_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        layout.addWidget(logo_label)
        
        # Engine name
        name_label = QLabel("Lupine Engine")
        name_label.setStyleSheet("font-size: 24px; font-weight: bold;")
        name_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        layout.addWidget(name_label)
        
        # Version
        version_label = QLabel("v0.1.0")
        version_label.setProperty("secondary", True)
        version_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        layout.addWidget(version_label)
        
        layout.addSpacing(30)
        
        # Action buttons
        new_project_btn = QPushButton("New Project")
        new_project_btn.clicked.connect(self._new_project)
        layout.addWidget(new_project_btn)
        
        open_project_btn = QPushButton("Open Project")
        open_project_btn.setProperty("secondary", True)
        open_project_btn.clicked.connect(self._open_project)
        layout.addWidget(open_project_btn)
        
        layout.addStretch()
        
        # Footer
        footer_label = QLabel("© 2024 Lupine Engine")
        footer_label.setProperty("secondary", True)
        footer_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        footer_label.setStyleSheet("font-size: 10px;")
        layout.addWidget(footer_label)
        
        panel.setLayout(layout)
        return panel
    
    def _create_right_panel(self) -> QWidget:
        """Create the right panel with recent projects list"""
        panel = QWidget()
        
        layout = QVBoxLayout()

        layout.setContentsMargins(5, 30, 5, 5)
        layout.setSpacing(5)
        
        # Title
        title_label = QLabel("Recent Projects")
        title_label.setStyleSheet("font-size: 20px; font-weight: bold;")
        title_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        layout.addWidget(title_label)

        # Recent projects list
        self.projects_list = QListWidget()
        self.projects_list.setSpacing(4)
        self.projects_list.setSelectionMode(QListWidget.SelectionMode.NoSelection)
        self.projects_list.setVerticalScrollMode(QListWidget.ScrollMode.ScrollPerPixel)
        self.projects_list.setStyleSheet("""
            QListWidget {
                background: transparent;
                border: none;
            }
            QListWidget::item {
                background: transparent;
                border: none;
            }
            QListWidget::item:selected, QListWidget::item:hover {
                background: transparent;
            }
        """)
        layout.addWidget(self.projects_list)
        
        # No projects message
        self.no_projects_label = QLabel("No recent projects.\nCreate a new project or open an existing one.")
        self.no_projects_label.setProperty("secondary", True)
        self.no_projects_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.no_projects_label.setStyleSheet("font-size: 14px; padding: 50px;")
        self.no_projects_label.hide()
        layout.addWidget(self.no_projects_label)
        
        panel.setLayout(layout)
        return panel
    
    def _load_recent_projects(self):
        """Load and display recent projects"""
        self.recent_projects = ProjectFile.load_recent_projects()
        self._update_projects_list()
    
    def _update_projects_list(self):
        """Update the recent projects list widget"""
        self.projects_list.clear()
        
        if not self.recent_projects:
            self.no_projects_label.show()
            self.projects_list.hide()
            return
        
        self.no_projects_label.hide()
        self.projects_list.show()
        
        for project in self.recent_projects:
            # Ensure project is a dictionary
            if not isinstance(project, dict):
                print(f"Warning: Invalid project entry in recent projects: {project}")
                continue
                
            project_path = project.get("path", "")
            project_name = project.get("name", "Unknown")
            last_opened = project.get("last_opened", "Unknown")
            
            # Check if project still exists
            if not os.path.exists(project_path):
                continue
            
            # Try to get icon, falling back to the engine default lupine icon
            icon_path = None
            try:
                project_data = ProjectFile.load_project(project_path)
                if project_data:
                    icon_path = project_data.get_icon_full_path()
            except Exception:
                pass

            if not icon_path:
                icon_path = ProjectFile.get_engine_default_icon_path()
            
            # Create list item
            item = QListWidgetItem(self.projects_list)
            item.setSizeHint(QSize(0, ITEM_HEIGHT))

            # Create custom widget
            widget = ProjectListItem(project_path, project_name, last_opened, icon_path)
            widget.clicked.connect(self._open_project_from_list)
            widget.remove_requested.connect(self._remove_from_recent)

            self.projects_list.addItem(item)
            self.projects_list.setItemWidget(item, widget)

    def _remove_from_recent(self, project_path: str):
        """Remove a project from the recent projects list"""
        recent = ProjectFile.load_recent_projects()
        recent = [p for p in recent if p.get("path") != project_path]
        ProjectFile.save_recent_projects(recent)
        self.recent_projects = recent
        self._update_projects_list()
    
    def _new_project(self):
        """Open new project dialog"""
        dialog = NewProjectDialog(self)
        
        if dialog.exec() == QDialog.DialogCode.Accepted:
            info = dialog.get_project_info()
            
            try:
                # Create the project
                project = ProjectFile.create_new_project(
                    info["name"],
                    info["location"],
                    info["creator"],
                    info.get("icon")
                )
                
                # Add to recent projects
                ProjectFile.add_to_recent_projects(project.path, project.name)
                
                # Open the project
                self._open_project_data(project)
                
            except Exception as e:
                QMessageBox.critical(
                    self,
                    "Error Creating Project",
                    f"Failed to create project:\n{str(e)}"
                )
    
    def _open_project(self):
        """Open file dialog to select a project"""
        file_path, _ = QFileDialog.getOpenFileName(
            self,
            "Open Project",
            str(Path.home() / "Documents"),
            "Lupine Project Files (*.lupine)"
        )
        
        if file_path:
            self._open_project_from_path(file_path)
    
    def _open_project_from_list(self, project_path: str):
        """Open a project from the recent projects list"""
        self._open_project_from_path(project_path)
    
    def _open_project_from_path(self, project_path: str):
        """Load and open a project from a file path"""
        try:
            project = ProjectFile.load_project(project_path)
            
            if project:
                # Add to recent projects
                ProjectFile.add_to_recent_projects(project.path, project.name)
                
                # Open the project
                self._open_project_data(project)
            else:
                QMessageBox.warning(
                    self,
                    "Failed to Open Project",
                    f"Could not load project from:\n{project_path}"
                )
                
        except Exception as e:
            QMessageBox.critical(
                self,
                "Error Opening Project",
                f"Failed to open project:\n{str(e)}"
            )
    
    def _open_project_data(self, project: ProjectData):
        """Open a loaded project"""
        print("=" * 60)
        print("PROJECT OPENED")
        print("=" * 60)
        print(f"Name: {project.name}")
        print(f"Path: {project.path}")
        print(f"Directory: {project.get_directory()}")
        print(f"Version: {project.version}")
        print(f"Creator: {project.creator}")
        print(f"Created: {project.created_date}")
        print(f"Last Modified: {project.last_modified}")
        print(f"Main Scene: {project.main_scene}")
        print(f"Window Size: {project.window_width}x{project.window_height}")
        print(f"Target FPS: {project.target_fps}")
        print("=" * 60)
        
        # Emit signal with project data
        self.project_selected.emit(project)
        
        # Close the project manager (as the editor would open)
        self.close()


# Import QDialog for the dialog
from PyQt6.QtWidgets import QDialog
