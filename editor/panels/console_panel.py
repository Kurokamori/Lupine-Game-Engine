"""
Console Panel
Display engine logs, errors, and debug output
"""

from PyQt6.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QPushButton,
                             QTextEdit, QComboBox, QLabel, QCheckBox)
from PyQt6.QtCore import Qt, pyqtSignal
from PyQt6.QtGui import QTextCursor, QColor
from .base_panel import EditorPanel


class ConsolePanel(EditorPanel):
    """Console output panel for logs and debug information"""
    
    # Signals
    command_executed = pyqtSignal(str)  # command string
    
    def __init__(self, parent=None):
        super().__init__("Console", parent)
        self.setObjectName("ConsolePanel")
    
    def _setup_panel(self):
        """Setup console panel UI"""
        # Toolbar
        toolbar_layout = QHBoxLayout()
        toolbar_layout.setContentsMargins(5, 5, 5, 5)
        
        clear_btn = QPushButton("Clear")
        clear_btn.setToolTip("Clear console output")
        toolbar_layout.addWidget(clear_btn)
        
        toolbar_layout.addSpacing(10)
        
        # Log level filter
        filter_label = QLabel("Filter:")
        toolbar_layout.addWidget(filter_label)
        
        self.log_level_combo = QComboBox()
        self.log_level_combo.addItems(["All", "Info", "Warning", "Error", "Debug"])
        toolbar_layout.addWidget(self.log_level_combo)
        
        toolbar_layout.addSpacing(10)
        
        # Checkboxes for log categories
        self.show_engine_logs = QCheckBox("Engine")
        self.show_engine_logs.setChecked(True)
        toolbar_layout.addWidget(self.show_engine_logs)
        
        self.show_script_logs = QCheckBox("Scripts")
        self.show_script_logs.setChecked(True)
        toolbar_layout.addWidget(self.show_script_logs)
        
        self.show_render_logs = QCheckBox("Rendering")
        self.show_render_logs.setChecked(True)
        toolbar_layout.addWidget(self.show_render_logs)
        
        toolbar_layout.addStretch()
        
        # Console output area
        self.console_output = QTextEdit()
        self.console_output.setReadOnly(True)
        self.console_output.setLineWrapMode(QTextEdit.LineWrapMode.NoWrap)
        
        # Add to layout
        self.content_layout.addLayout(toolbar_layout)
        self.content_layout.addWidget(self.console_output)
    
    def log_message(self, message: str, level: str = "Info"):
        """Add a log message to the console"""
        cursor = self.console_output.textCursor()
        cursor.movePosition(QTextCursor.MoveOperation.End)
        
        # Color based on level
        if level == "Error":
            self.console_output.setTextColor(QColor(255, 100, 100))
        elif level == "Warning":
            self.console_output.setTextColor(QColor(255, 200, 100))
        elif level == "Debug":
            self.console_output.setTextColor(QColor(150, 150, 150))
        else:
            self.console_output.setTextColor(QColor(255, 255, 255))
        
        self.console_output.append(f"[{level}] {message}")
        
        # Scroll to bottom
        cursor.movePosition(QTextCursor.MoveOperation.End)
        self.console_output.setTextCursor(cursor)
