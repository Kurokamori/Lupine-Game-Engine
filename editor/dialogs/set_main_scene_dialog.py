"""
Set Main Scene Dialog
Dialog shown when user tries to play game with no main scene set
"""

from PyQt6.QtWidgets import (QDialog, QVBoxLayout, QHBoxLayout, QPushButton,
                             QLabel)
from PyQt6.QtCore import Qt
from PyQt6.QtGui import QIcon


class SetMainSceneDialog(QDialog):
    """Dialog for setting the main scene when none is set"""

    def __init__(self, has_open_scene: bool, current_scene_name: str = "", parent=None):
        super().__init__(parent)
        self.has_open_scene = has_open_scene
        self.current_scene_name = current_scene_name
        self.set_as_main = False

        self.setWindowTitle("No Main Scene Set")
        self.setMinimumWidth(400)
        self.setModal(True)

        self._setup_ui()

    def _setup_ui(self):
        """Setup the dialog UI"""
        layout = QVBoxLayout()
        layout.setSpacing(16)

        # Icon and message
        message_layout = QHBoxLayout()
        
        # Warning icon (using text for now, could use QIcon later)
        icon_label = QLabel("⚠️")
        icon_label.setStyleSheet("font-size: 32px;")
        message_layout.addWidget(icon_label)
        
        # Message text
        message_text = QLabel(
            "No main scene is set in the project settings.\n\n"
            "The main scene is the entry point for your game when you click 'Play Game'."
        )
        message_text.setWordWrap(True)
        message_layout.addWidget(message_text, 1)
        
        layout.addLayout(message_layout)

        # Current scene info (if available)
        if self.has_open_scene:
            info_label = QLabel(f"Current scene: <b>{self.current_scene_name}</b>")
            info_label.setStyleSheet("padding: 8px; background-color: rgba(255, 255, 255, 0.05); border-radius: 4px;")
            layout.addWidget(info_label)

        # Buttons
        button_layout = QHBoxLayout()
        button_layout.addStretch()

        # Set Current Scene button (only enabled if scene is open)
        self.set_scene_btn = QPushButton("Set Current Scene as Main")
        self.set_scene_btn.setProperty("success", True)
        self.set_scene_btn.setEnabled(self.has_open_scene)
        if not self.has_open_scene:
            self.set_scene_btn.setToolTip("No scene is currently open")
        self.set_scene_btn.clicked.connect(self._on_set_scene)
        button_layout.addWidget(self.set_scene_btn)

        # Close button
        close_btn = QPushButton("Close")
        close_btn.setProperty("secondary", True)
        close_btn.clicked.connect(self.reject)
        button_layout.addWidget(close_btn)

        layout.addLayout(button_layout)

        self.setLayout(layout)

    def _on_set_scene(self):
        """Handle Set Current Scene button click"""
        self.set_as_main = True
        self.accept()

    def should_set_as_main(self) -> bool:
        """Returns True if user chose to set current scene as main"""
        return self.set_as_main

