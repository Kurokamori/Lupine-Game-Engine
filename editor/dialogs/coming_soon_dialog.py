"""
Coming Soon Dialog
A simple dialog to show for unimplemented features
"""

from PyQt6.QtWidgets import QDialog, QVBoxLayout, QLabel, QPushButton
from PyQt6.QtCore import Qt
from PyQt6.QtGui import QFont


class ComingSoonDialog(QDialog):
    """A simple dialog that shows 'Coming Soon' message"""

    def __init__(self, tool_name: str = "This Feature", parent=None):
        super().__init__(parent)
        self.tool_name = tool_name
        self.setWindowTitle(f"{tool_name}")
        self.setModal(True)
        self.setMinimumSize(400, 200)

        self._setup_ui()

    def _setup_ui(self):
        """Setup the dialog UI"""
        layout = QVBoxLayout(self)
        layout.setSpacing(20)
        layout.setContentsMargins(30, 30, 30, 30)

        # Coming Soon label
        coming_soon_label = QLabel("Coming Soon")
        coming_soon_font = QFont()
        coming_soon_font.setPointSize(24)
        coming_soon_font.setBold(True)
        coming_soon_label.setFont(coming_soon_font)
        coming_soon_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        layout.addWidget(coming_soon_label)

        # Feature name label
        feature_label = QLabel(f"{self.tool_name}")
        feature_font = QFont()
        feature_font.setPointSize(12)
        feature_label.setFont(feature_font)
        feature_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        feature_label.setWordWrap(True)
        layout.addWidget(feature_label)

        # Info label
        info_label = QLabel("This feature is planned for a future release.")
        info_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        layout.addWidget(info_label)

        layout.addStretch()

        # OK button
        ok_button = QPushButton("OK")
        ok_button.setFixedWidth(100)
        ok_button.clicked.connect(self.accept)
        layout.addWidget(ok_button, alignment=Qt.AlignmentFlag.AlignCenter)
