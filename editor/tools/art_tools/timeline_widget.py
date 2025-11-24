"""
Timeline Widget for Animation
Displays frames for each layer with controls for frame manipulation
"""

from PyQt6.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QLabel,
                             QPushButton, QScrollArea, QFrame, QSpinBox,
                             QDialog, QLineEdit, QDialogButtonBox, QFormLayout,
                             QComboBox)
from PyQt6.QtCore import Qt, pyqtSignal, QRect, QPoint
from PyQt6.QtGui import QPainter, QColor, QPen, QBrush, QFont

import sys
from pathlib import Path
editor_path = Path(__file__).parent.parent.parent
if str(editor_path) not in sys.path:
    sys.path.insert(0, str(editor_path))

from theme import get_theme_manager


class TimelineWidget(QWidget):
    """Timeline widget for animation frames"""

    frame_selected = pyqtSignal(int)  # Emitted when frame is selected
    frame_changed = pyqtSignal()  # Emitted when frames are modified

    def __init__(self, parent=None):
        super().__init__(parent)
        self.canvas = None
        self.current_frame = 0
        self.frame_width = 60
        self.frame_height = 50
        self.layer_name_width = 100

        self.setMinimumHeight(150)

        # Get theme
        theme_manager = get_theme_manager()
        theme = theme_manager.get_current_theme()
        self.setStyleSheet(f"background-color: {theme.colors.surface};")

        # Setup UI
        layout = QVBoxLayout()
        layout.setContentsMargins(0, 0, 0, 0)

        # Controls
        controls = QHBoxLayout()

        self.new_frame_btn = QPushButton("New Frame")
        controls.addWidget(self.new_frame_btn)

        self.duplicate_frame_btn = QPushButton("Duplicate")
        controls.addWidget(self.duplicate_frame_btn)

        self.delete_frame_btn = QPushButton("Delete")
        controls.addWidget(self.delete_frame_btn)

        self.move_left_btn = QPushButton("Move Left")
        controls.addWidget(self.move_left_btn)

        self.move_right_btn = QPushButton("Move Right")
        controls.addWidget(self.move_right_btn)

        controls.addStretch()

        self.frame_label = QLabel("Frame: 1/1")
        controls.addWidget(self.frame_label)

        layout.addLayout(controls)

        # Scroll area for frames
        scroll = QScrollArea()
        scroll.setWidgetResizable(False)
        scroll.setHorizontalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAsNeeded)
        scroll.setVerticalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAsNeeded)

        self.frame_view = TimelineFrameView(self)
        scroll.setWidget(self.frame_view)

        layout.addWidget(scroll, 1)

        self.setLayout(layout)

    def set_canvas(self, canvas):
        """Set the pixel canvas"""
        self.canvas = canvas
        self.frame_view.canvas = canvas

        # Force initial size update
        if self.canvas:
            size = self.frame_view.sizeHint()
            self.frame_view.setMinimumSize(size)
            self.frame_view.resize(size)

        self.update_display()

    def update_display(self):
        """Update the timeline display"""
        if self.canvas:
            self.current_frame = self.canvas.current_frame
            frame_count = self.canvas.get_frame_count()
            self.frame_label.setText(f"Frame: {self.current_frame + 1}/{frame_count}")
            self.frame_view.current_frame = self.current_frame

            # Update the size of the frame view
            size = self.frame_view.sizeHint()
            self.frame_view.setMinimumSize(size)
            self.frame_view.resize(size)

            self.frame_view.update()
            self.frame_view.updateGeometry()

    def on_frame_clicked(self, frame_index: int):
        """Handle frame click"""
        self.current_frame = frame_index
        self.frame_selected.emit(frame_index)
        self.update_display()


class TimelineFrameView(QWidget):
    """Widget that displays the frame grid"""

    def __init__(self, timeline: TimelineWidget):
        super().__init__()
        self.timeline = timeline
        self.canvas = None
        self.current_frame = 0
        self.frame_width = 60
        self.frame_height = 50
        self.layer_name_width = 100

        # Set a reasonable initial minimum size
        self.setMinimumSize(400, 100)
        self.setMouseTracking(True)

    def sizeHint(self):
        """Calculate size based on frames and layers"""
        if not self.canvas:
            return super().sizeHint()

        frame_count = self.canvas.get_frame_count()
        layer_count = len(self.canvas.layers)

        width = self.layer_name_width + frame_count * self.frame_width + 20
        height = layer_count * self.frame_height + 20

        from PyQt6.QtCore import QSize
        return QSize(width, height)

    def paintEvent(self, event):
        """Paint the timeline"""
        if not self.canvas:
            return

        theme_manager = get_theme_manager()
        theme = theme_manager.get_current_theme()
        c = theme.colors

        painter = QPainter(self)
        painter.fillRect(self.rect(), QColor(c.surface))

        frame_count = self.canvas.get_frame_count()

        # Draw layer names and frames
        for layer_idx, layer in enumerate(self.canvas.layers):
            y = layer_idx * self.frame_height + 10

            # Draw layer name
            painter.setPen(QColor(c.text_primary))
            font = QFont()
            font.setPointSize(9)
            painter.setFont(font)
            painter.drawText(5, y + 30, layer.name[:15])

            # Draw frames
            for frame_idx in range(frame_count):
                x = self.layer_name_width + frame_idx * self.frame_width

                # Frame exists for this layer?
                has_content = frame_idx < len(layer.frames)

                # Background - only fill current frame
                is_current = frame_idx == self.current_frame

                if is_current:
                    # Current frame - use accent color background
                    bg_color = QColor(c.accent_color)
                    bg_color.setAlpha(80)
                    painter.fillRect(x, y, self.frame_width - 2, self.frame_height - 2, bg_color)
                else:
                    # All other frames - same background (no fill)
                    painter.fillRect(x, y, self.frame_width - 2, self.frame_height - 2, QColor(c.surface))

                # Border - thicker for current frame
                if is_current:
                    painter.setPen(QPen(QColor(c.accent_color), 2))
                else:
                    painter.setPen(QPen(QColor(c.border), 1))
                painter.drawRect(x, y, self.frame_width - 2, self.frame_height - 2)

                # Frame number
                painter.setPen(QColor(c.text_primary) if is_current else QColor(c.text_secondary))
                font = QFont()
                font.setBold(is_current)
                font.setPointSize(9)
                painter.setFont(font)
                painter.drawText(x + 5, y + 15, str(frame_idx + 1))

                # Thumbnail (simplified - just show if content exists)
                # Reset painter state first
                painter.setBrush(Qt.BrushStyle.NoBrush)
                painter.setPen(QPen(QColor(c.border), 1))

                if has_content and frame_idx < len(layer.frames):
                    frame = layer.frames[frame_idx]
                    if not frame.canvas.isNull():
                        # Check if frame actually has pixel data (not just transparent)
                        img = frame.canvas.toImage()
                        has_pixels = False
                        # Quick check - just sample a few pixels
                        for check_y in range(0, min(img.height(), 10), 2):
                            for check_x in range(0, min(img.width(), 10), 2):
                                if img.pixelColor(check_x, check_y).alpha() > 0:
                                    has_pixels = True
                                    break
                            if has_pixels:
                                break

                        if has_pixels:
                            # Draw a small filled circle to indicate content
                            painter.setBrush(QBrush(QColor(c.accent_color)))
                            painter.setPen(Qt.PenStyle.NoPen)
                            painter.drawEllipse(x + self.frame_width // 2 - 5, y + 25, 10, 10)
                            # Reset brush
                            painter.setBrush(Qt.BrushStyle.NoBrush)

    def mousePressEvent(self, event):
        """Handle mouse press"""
        if not self.canvas:
            return

        frame_count = self.canvas.get_frame_count()

        # Calculate which frame was clicked
        for frame_idx in range(frame_count):
            x = self.layer_name_width + frame_idx * self.frame_width
            for layer_idx in range(len(self.canvas.layers)):
                y = layer_idx * self.frame_height + 10

                rect = QRect(x, y, self.frame_width - 2, self.frame_height - 2)
                if rect.contains(event.pos()):
                    self.timeline.on_frame_clicked(frame_idx)
                    return
