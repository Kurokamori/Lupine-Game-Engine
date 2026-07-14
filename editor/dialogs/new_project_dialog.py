"""
Lupine Engine New Project Dialog
Dialog for creating new projects with name and location selection
"""

from PyQt6.QtWidgets import (QDialog, QVBoxLayout, QHBoxLayout, QLabel,
                             QLineEdit, QPushButton, QFileDialog, QMessageBox,
                             QFrame)
from PyQt6.QtCore import Qt
from PyQt6.QtGui import QPixmap
from pathlib import Path
import os

from project_file import ProjectFile


class NewProjectDialog(QDialog):
    """Dialog for creating a new Lupine Engine project"""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("New Project")
        self.setModal(True)
        self.setMinimumWidth(500)

        self.project_name = ""
        self.project_location = ""
        self.creator_name = ""
        self.icon_path = ""

        self._setup_ui()
        self._set_default_location()
        self._set_default_icon()
    
    def _setup_ui(self):
        """Setup the dialog UI"""
        layout = QVBoxLayout()
        layout.setSpacing(15)
        
        # Title
        title_label = QLabel("Create New Project")
        title_label.setStyleSheet("font-size: 18px; font-weight: bold;")
        layout.addWidget(title_label)
        
        # Project Name
        name_label = QLabel("Project Name:")
        self.name_edit = QLineEdit()
        self.name_edit.setPlaceholderText("Enter project name...")
        self.name_edit.textChanged.connect(self._validate_inputs)
        layout.addWidget(name_label)
        layout.addWidget(self.name_edit)
        
        # Creator Name (optional)
        creator_label = QLabel("Creator Name (Optional):")
        self.creator_edit = QLineEdit()
        self.creator_edit.setPlaceholderText("Your name...")
        layout.addWidget(creator_label)
        layout.addWidget(self.creator_edit)

        # Project Icon
        icon_label = QLabel("Project Icon:")
        layout.addWidget(icon_label)

        icon_layout = QHBoxLayout()
        icon_layout.setSpacing(12)

        self.icon_preview = QLabel()
        self.icon_preview.setFixedSize(64, 64)
        self.icon_preview.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.icon_preview.setFrameShape(QFrame.Shape.StyledPanel)
        icon_layout.addWidget(self.icon_preview)

        icon_buttons_layout = QVBoxLayout()
        icon_buttons_layout.setSpacing(6)

        choose_icon_button = QPushButton("Choose Icon...")
        choose_icon_button.setProperty("secondary", True)
        choose_icon_button.clicked.connect(self._browse_icon)
        icon_buttons_layout.addWidget(choose_icon_button)

        reset_icon_button = QPushButton("Use Default")
        reset_icon_button.setProperty("secondary", True)
        reset_icon_button.clicked.connect(self._set_default_icon)
        icon_buttons_layout.addWidget(reset_icon_button)

        icon_layout.addLayout(icon_buttons_layout)
        icon_layout.addStretch()
        layout.addLayout(icon_layout)

        # Project Location
        location_label = QLabel("Project Location:")
        layout.addWidget(location_label)
        
        location_layout = QHBoxLayout()
        self.location_edit = QLineEdit()
        self.location_edit.setPlaceholderText("Select project location...")
        self.location_edit.textChanged.connect(self._validate_inputs)
        location_layout.addWidget(self.location_edit)
        
        browse_button = QPushButton("Browse...")
        browse_button.setProperty("secondary", True)
        browse_button.clicked.connect(self._browse_location)
        location_layout.addWidget(browse_button)
        
        layout.addLayout(location_layout)
        
        # Full path preview
        self.path_preview = QLabel()
        self.path_preview.setProperty("secondary", True)
        self.path_preview.setWordWrap(True)
        self.path_preview.setStyleSheet("font-style: italic;")
        layout.addWidget(self.path_preview)
        
        # Spacer
        layout.addStretch()
        
        # Buttons
        button_layout = QHBoxLayout()
        button_layout.addStretch()
        
        cancel_button = QPushButton("Cancel")
        cancel_button.setProperty("secondary", True)
        cancel_button.clicked.connect(self.reject)
        button_layout.addWidget(cancel_button)
        
        self.create_button = QPushButton("Create Project")
        self.create_button.setProperty("success", True)
        self.create_button.clicked.connect(self._create_project)
        self.create_button.setEnabled(False)
        button_layout.addWidget(self.create_button)
        
        layout.addLayout(button_layout)
        
        self.setLayout(layout)
    
    def _set_default_location(self):
        """Set default project location to Documents/LupineProjects"""
        documents = Path.home() / "Documents"
        default_location = documents / "LupineProjects"
        self.location_edit.setText(str(default_location))
    
    def _set_default_icon(self):
        """Reset the icon selection to the engine's default icon"""
        default_icon = ProjectFile.get_engine_default_icon_path()
        self.icon_path = default_icon or ""
        self._update_icon_preview()

    def _browse_icon(self):
        """Open file dialog to select a project icon"""
        file_path, _ = QFileDialog.getOpenFileName(
            self,
            "Select Project Icon",
            str(Path.home()),
            "Image Files (*.png *.jpg *.jpeg *.bmp *.ico)"
        )

        if file_path:
            self.icon_path = file_path
            self._update_icon_preview()

    def _update_icon_preview(self):
        """Refresh the icon preview thumbnail"""
        if self.icon_path and os.path.exists(self.icon_path):
            pixmap = QPixmap(self.icon_path).scaled(
                64, 64,
                Qt.AspectRatioMode.KeepAspectRatio,
                Qt.TransformationMode.SmoothTransformation
            )
            self.icon_preview.setPixmap(pixmap)
            self.icon_preview.setText("")
        else:
            self.icon_preview.setPixmap(QPixmap())
            self.icon_preview.setText("🎮")
            self.icon_preview.setStyleSheet("font-size: 28px;")

    def _browse_location(self):
        """Open file dialog to select project location"""
        current_location = self.location_edit.text()
        if not current_location:
            current_location = str(Path.home() / "Documents")
        
        folder = QFileDialog.getExistingDirectory(
            self,
            "Select Project Location",
            current_location,
            QFileDialog.Option.ShowDirsOnly
        )
        
        if folder:
            self.location_edit.setText(folder)
    
    def _validate_inputs(self):
        """Validate user inputs and update UI"""
        name = self.name_edit.text().strip()
        location = self.location_edit.text().strip()
        
        # Check if inputs are valid
        valid = bool(name and location)
        
        # Update full path preview
        if name and location:
            full_path = Path(location) / name
            self.path_preview.setText(f"Project will be created at: {full_path}")
            
            # Check if project already exists
            if full_path.exists():
                self.path_preview.setText(
                    f"⚠ Warning: Folder already exists: {full_path}"
                )
                self.path_preview.setStyleSheet("color: orange; font-style: italic;")
            else:
                self.path_preview.setStyleSheet("font-style: italic;")
        else:
            self.path_preview.setText("")
        
        self.create_button.setEnabled(valid)
    
    def _validate_project_name(self, name: str) -> bool:
        """Check if project name is valid"""
        if not name:
            return False
        
        # Check for invalid characters
        invalid_chars = ['<', '>', ':', '"', '/', '\\', '|', '?', '*']
        for char in invalid_chars:
            if char in name:
                return False
        
        return True
    
    def _create_project(self):
        """Validate and create the project"""
        name = self.name_edit.text().strip()
        location = self.location_edit.text().strip()
        creator = self.creator_edit.text().strip()
        
        # Validate project name
        if not self._validate_project_name(name):
            QMessageBox.warning(
                self,
                "Invalid Project Name",
                "Project name contains invalid characters.\n"
                "Avoid using: < > : \" / \\ | ? *"
            )
            return
        
        # Check if location exists
        location_path = Path(location)
        if not location_path.exists():
            reply = QMessageBox.question(
                self,
                "Create Directory",
                f"The directory '{location}' does not exist.\n"
                "Do you want to create it?",
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
            )
            
            if reply != QMessageBox.StandardButton.Yes:
                return
        
        # Check if project folder already exists
        project_path = location_path / name
        if project_path.exists():
            reply = QMessageBox.question(
                self,
                "Folder Exists",
                f"A folder named '{name}' already exists at this location.\n"
                "Do you want to use it anyway?",
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
            )
            
            if reply != QMessageBox.StandardButton.Yes:
                return
        
        # Store values
        self.project_name = name
        self.project_location = location
        self.creator_name = creator
        
        self.accept()
    
    def get_project_info(self):
        """Get the project information entered by the user"""
        return {
            "name": self.project_name,
            "location": self.project_location,
            "creator": self.creator_name,
            "icon": self.icon_path
        }
