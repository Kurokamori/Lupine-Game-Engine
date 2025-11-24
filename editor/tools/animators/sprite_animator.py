"""
Sprite Animator Tool
A comprehensive tool for creating and managing sprite animations
Supports sprite sheets and image sequences
"""

from PyQt6.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QLabel,
                             QPushButton, QListWidget, QListWidgetItem,
                             QFileDialog, QMessageBox, QDialog, QFormLayout,
                             QLineEdit, QComboBox, QDialogButtonBox, QSpinBox,
                             QDoubleSpinBox, QSplitter, QGroupBox, QGridLayout,
                             QScrollArea, QRadioButton, QButtonGroup, QFrame)
from PyQt6.QtCore import Qt, QTimer, pyqtSignal, QSize, QRect
from PyQt6.QtGui import QPixmap, QImage, QPainter, QColor, QPen
from pathlib import Path
import json
import sys
import uuid

# Import base panel
editor_path = Path(__file__).parent.parent.parent
if str(editor_path) not in sys.path:
    sys.path.insert(0, str(editor_path))

from panels.base_panel import EditorPanel


class SpriteSlice:
    """Represents a single sprite slice"""
    def __init__(self, rect: QRect, index: int, source_image_path: str = None):
        self.rect = rect  # QRect defining the slice area
        self.index = index
        self.source_image_path = source_image_path


class Animation:
    """Represents an animation with frames"""
    def __init__(self, name: str):
        self.name = name
        self.frames = []  # List of sprite slice indices
        self.loop_behavior = "linear"  # none, linear, pingpong
        self.speed = 10.0  # Frames per second


class SpriteSliceWidget(QWidget):
    """Widget for displaying and selecting sprite slices"""

    slice_clicked = pyqtSignal(int)  # Emits slice index when clicked

    def __init__(self, parent=None):
        super().__init__(parent)
        self.sprites = []  # List of SpriteSlice objects
        self.images = []  # List of QPixmap objects
        self.selected_indices = set()
        self.hover_index = -1
        self.transformation_mode = Qt.TransformationMode.SmoothTransformation
        # Hook for per-asset texture filtering (to be implemented later)
        self.asset_texture_filter = None

        # Grid layout settings
        self.display_cols = 8  # How many columns to display sprites in
        self.display_rows = None  # Auto-calculated

        self.setMinimumSize(400, 300)
        self.setMouseTracking(True)

    def set_display_grid(self, cols: int, rows: int = None):
        """Set the display grid layout"""
        self.display_cols = cols if cols > 0 else 8
        self.display_rows = rows
        self.update()

    def set_texture_filtering(self, filter_mode: str):
        """Set texture filtering mode based on project settings"""
        # Map texture filtering to Qt transformation modes
        filter_map = {
            "nearest": Qt.TransformationMode.FastTransformation,
            "bilinear": Qt.TransformationMode.SmoothTransformation,
            "cubic": Qt.TransformationMode.SmoothTransformation
        }
        self.transformation_mode = filter_map.get(filter_mode.lower(), Qt.TransformationMode.SmoothTransformation)
        self.update()

    def set_asset_texture_filter(self, filter_mode: str):
        """
        Set per-asset texture filtering (overrides project settings)
        Hook for future asset metadata system
        """
        if filter_mode:
            filter_map = {
                "nearest": Qt.TransformationMode.FastTransformation,
                "bilinear": Qt.TransformationMode.SmoothTransformation,
                "cubic": Qt.TransformationMode.SmoothTransformation
            }
            self.asset_texture_filter = filter_map.get(filter_mode.lower())
            self.transformation_mode = self.asset_texture_filter
            self.update()
        else:
            self.asset_texture_filter = None

    def load_images(self, image_paths: list):
        """Load multiple images as individual slices"""
        self.sprites.clear()
        self.images.clear()
        self.selected_indices.clear()

        for idx, path in enumerate(image_paths):
            pixmap = QPixmap(str(path))
            if not pixmap.isNull():
                self.images.append(pixmap)
                # Each image is its own slice
                rect = QRect(0, 0, pixmap.width(), pixmap.height())
                self.sprites.append(SpriteSlice(rect, idx, str(path)))

        self.update()

    def load_sprite_sheet(self, image_path: str, cols: int, rows: int):
        """Load a sprite sheet and slice it into a grid"""
        pixmap = QPixmap(str(image_path))
        if pixmap.isNull():
            return

        self.sprites.clear()
        self.images = [pixmap]
        self.selected_indices.clear()

        sprite_width = pixmap.width() // cols
        sprite_height = pixmap.height() // rows

        idx = 0
        for row in range(rows):
            for col in range(cols):
                x = col * sprite_width
                y = row * sprite_height
                rect = QRect(x, y, sprite_width, sprite_height)
                self.sprites.append(SpriteSlice(rect, idx, str(image_path)))
                idx += 1

        self.update()

    def load_sprite_sheet_by_size(self, image_path: str, sprite_width: int, sprite_height: int):
        """Load a sprite sheet and slice it by sprite dimensions"""
        pixmap = QPixmap(str(image_path))
        if pixmap.isNull():
            return

        self.sprites.clear()
        self.images = [pixmap]
        self.selected_indices.clear()

        cols = pixmap.width() // sprite_width
        rows = pixmap.height() // sprite_height

        idx = 0
        for row in range(rows):
            for col in range(cols):
                x = col * sprite_width
                y = row * sprite_height
                rect = QRect(x, y, sprite_width, sprite_height)
                self.sprites.append(SpriteSlice(rect, idx, str(image_path)))
                idx += 1

        self.update()

    def get_sprite_count(self):
        """Get the number of sprites"""
        return len(self.sprites)

    def get_sprite_pixmap(self, index: int) -> QPixmap:
        """Get a pixmap for a specific sprite slice"""
        if index < 0 or index >= len(self.sprites):
            return QPixmap()

        sprite = self.sprites[index]

        # If each image is its own slice
        if len(self.images) > 1:
            return self.images[index]

        # Extract from sprite sheet
        if len(self.images) == 1:
            return self.images[0].copy(sprite.rect)

        return QPixmap()

    def paintEvent(self, event):
        """Draw the sprite slices"""
        painter = QPainter(self)

        if not self.sprites:
            painter.drawText(self.rect(), Qt.AlignmentFlag.AlignCenter,
                           "Load images or a sprite sheet")
            return

        # Calculate grid layout
        grid_cols = self.display_cols
        sprite_size = 64
        padding = 10

        # Use asset-specific or project-wide texture filtering
        transform_mode = self.asset_texture_filter if self.asset_texture_filter else self.transformation_mode

        for idx, sprite in enumerate(self.sprites):
            row = idx // grid_cols
            col = idx % grid_cols

            x = col * (sprite_size + padding) + padding
            y = row * (sprite_size + padding) + padding

            # Draw sprite
            pixmap = self.get_sprite_pixmap(idx)
            if not pixmap.isNull():
                scaled = pixmap.scaled(sprite_size, sprite_size,
                                      Qt.AspectRatioMode.KeepAspectRatio,
                                      transform_mode)
                painter.drawPixmap(x, y, scaled)

            # Draw selection/hover border
            if idx in self.selected_indices:
                painter.setPen(QPen(QColor(0, 120, 215), 3))
                painter.drawRect(x - 2, y - 2, sprite_size + 4, sprite_size + 4)
            elif idx == self.hover_index:
                painter.setPen(QPen(QColor(100, 100, 100), 2))
                painter.drawRect(x - 1, y - 1, sprite_size + 2, sprite_size + 2)

            # Draw index
            painter.setPen(QColor(255, 255, 255))
            painter.drawText(x + 2, y + 12, str(idx))

        # Update widget size
        total_rows = (len(self.sprites) + grid_cols - 1) // grid_cols
        min_height = total_rows * (sprite_size + padding) + padding
        self.setMinimumHeight(min_height)

    def mousePressEvent(self, event):
        """Handle mouse click to select sprite"""
        if event.button() == Qt.MouseButton.LeftButton:
            index = self._get_sprite_at_pos(event.pos())
            if index >= 0:
                self.slice_clicked.emit(index)

    def mouseMoveEvent(self, event):
        """Handle mouse move for hover effect"""
        old_hover = self.hover_index
        self.hover_index = self._get_sprite_at_pos(event.pos())
        if old_hover != self.hover_index:
            self.update()

    def _get_sprite_at_pos(self, pos):
        """Get sprite index at mouse position"""
        grid_cols = self.display_cols
        sprite_size = 64
        padding = 10

        for idx in range(len(self.sprites)):
            row = idx // grid_cols
            col = idx % grid_cols

            x = col * (sprite_size + padding) + padding
            y = row * (sprite_size + padding) + padding

            if (x <= pos.x() <= x + sprite_size and
                y <= pos.y() <= y + sprite_size):
                return idx

        return -1


class AnimationPreviewWidget(QWidget):
    """Widget for previewing animations"""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.sprite_widget = None
        self.current_animation = None
        self.current_frame_index = 0
        self.is_playing = False
        self.is_reverse = False  # For pingpong
        self.transformation_mode = Qt.TransformationMode.SmoothTransformation

        self.timer = QTimer()
        self.timer.timeout.connect(self._advance_frame)

        self.setMinimumSize(200, 200)

    def set_texture_filtering(self, filter_mode: str):
        """Set texture filtering mode for preview"""
        filter_map = {
            "nearest": Qt.TransformationMode.FastTransformation,
            "bilinear": Qt.TransformationMode.SmoothTransformation,
            "cubic": Qt.TransformationMode.SmoothTransformation
        }
        self.transformation_mode = filter_map.get(filter_mode.lower(), Qt.TransformationMode.SmoothTransformation)
        self.update()

    def set_sprite_widget(self, sprite_widget: SpriteSliceWidget):
        """Set the sprite widget to get sprite data from"""
        self.sprite_widget = sprite_widget

    def set_animation(self, animation: Animation):
        """Set the animation to preview"""
        self.current_animation = animation
        self.current_frame_index = 0
        self.is_reverse = False
        self._update_timer()
        self.update()

    def play(self):
        """Start playing the animation"""
        if self.current_animation and len(self.current_animation.frames) > 0:
            self.is_playing = True
            self.timer.start()

    def pause(self):
        """Pause the animation"""
        self.is_playing = False
        self.timer.stop()

    def stop(self):
        """Stop the animation and reset to first frame"""
        self.is_playing = False
        self.timer.stop()
        self.current_frame_index = 0
        self.is_reverse = False
        self.update()

    def _update_timer(self):
        """Update timer interval based on animation speed"""
        if self.current_animation and self.current_animation.speed > 0:
            interval = int(1000.0 / self.current_animation.speed)
            self.timer.setInterval(interval)

    def _advance_frame(self):
        """Advance to next frame"""
        if not self.current_animation or len(self.current_animation.frames) == 0:
            return

        loop_behavior = self.current_animation.loop_behavior
        frame_count = len(self.current_animation.frames)

        if loop_behavior == "none":
            self.current_frame_index += 1
            if self.current_frame_index >= frame_count:
                self.current_frame_index = frame_count - 1
                self.pause()
        elif loop_behavior == "linear":
            self.current_frame_index = (self.current_frame_index + 1) % frame_count
        elif loop_behavior == "pingpong":
            if self.is_reverse:
                self.current_frame_index -= 1
                if self.current_frame_index <= 0:
                    self.current_frame_index = 0
                    self.is_reverse = False
            else:
                self.current_frame_index += 1
                if self.current_frame_index >= frame_count - 1:
                    self.current_frame_index = frame_count - 1
                    self.is_reverse = True

        self.update()

    def paintEvent(self, event):
        """Draw the current frame"""
        painter = QPainter(self)
        painter.fillRect(self.rect(), QColor(50, 50, 50))

        if (not self.current_animation or
            not self.sprite_widget or
            len(self.current_animation.frames) == 0):
            painter.setPen(QColor(200, 200, 200))
            painter.drawText(self.rect(), Qt.AlignmentFlag.AlignCenter,
                           "No animation to preview")
            return

        # Get current sprite
        sprite_index = self.current_animation.frames[self.current_frame_index]
        pixmap = self.sprite_widget.get_sprite_pixmap(sprite_index)

        if not pixmap.isNull():
            # Use same filtering as sprite widget
            transform_mode = self.sprite_widget.asset_texture_filter if self.sprite_widget.asset_texture_filter else self.transformation_mode

            # Scale to fit widget while maintaining aspect ratio
            scaled = pixmap.scaled(self.width() - 20, self.height() - 20,
                                  Qt.AspectRatioMode.KeepAspectRatio,
                                  transform_mode)

            # Center the sprite
            x = (self.width() - scaled.width()) // 2
            y = (self.height() - scaled.height()) // 2
            painter.drawPixmap(x, y, scaled)

        # Draw frame info
        painter.setPen(QColor(200, 200, 200))
        info = f"Frame {self.current_frame_index + 1}/{len(self.current_animation.frames)}"
        painter.drawText(10, self.height() - 10, info)


class NewAnimationDialog(QDialog):
    """Dialog for creating a new animation"""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("New Animation")
        self.setModal(True)
        self.setMinimumWidth(400)

        layout = QVBoxLayout()

        form_layout = QFormLayout()

        self.name_edit = QLineEdit()
        self.name_edit.setPlaceholderText("animation_name")
        form_layout.addRow("Name:", self.name_edit)

        self.loop_combo = QComboBox()
        self.loop_combo.addItems(["none", "linear", "pingpong"])
        self.loop_combo.setCurrentText("linear")
        form_layout.addRow("Loop Behavior:", self.loop_combo)

        self.speed_spin = QDoubleSpinBox()
        self.speed_spin.setMinimum(0.1)
        self.speed_spin.setMaximum(120.0)
        self.speed_spin.setValue(10.0)
        self.speed_spin.setSuffix(" FPS")
        form_layout.addRow("Speed:", self.speed_spin)

        layout.addLayout(form_layout)

        buttons = QDialogButtonBox(
            QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel
        )
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)

        self.setLayout(layout)

    def get_animation_data(self):
        """Get the animation data"""
        return {
            'name': self.name_edit.text() if self.name_edit.text() else 'new_animation',
            'loop_behavior': self.loop_combo.currentText(),
            'speed': self.speed_spin.value()
        }


class SpriteAnimatorTool(EditorPanel):
    """Sprite Animation tool for creating sprite animations"""

    def __init__(self, parent=None, project_root=None):
        self.project_root = project_root
        self.current_file = None
        self.animations = {}  # name -> Animation
        self.selected_animation = None
        self.loaded_image_paths = []
        self.asset_uuid = None
        self.current_sheet_path = None
        self.sprite_config = {}  # Track how sprites were loaded
        self.is_aseprite_import = False  # Track if loaded from Aseprite

        # Get project settings for texture filtering
        self.project = None
        if parent and hasattr(parent, 'project'):
            self.project = parent.project

        super().__init__("Sprite Animator", parent)
        self.setObjectName("SpriteAnimatorTool")

        # Apply texture filtering from project settings
        if self.project:
            self._apply_texture_filtering()

    def _apply_texture_filtering(self):
        """Apply texture filtering from project settings"""
        # Get the project reference if not already set
        if not self.project and self.parent() and hasattr(self.parent(), 'project'):
            self.project = self.parent().project

        if self.project and hasattr(self.project, 'texture_filtering'):
            filter_mode = self.project.texture_filtering
            self.sprite_widget.set_texture_filtering(filter_mode)
            self.preview_widget.set_texture_filtering(filter_mode)

            # TODO: Check for per-asset texture filtering when asset system is implemented
            # if self.current_file and asset_has_custom_filtering(self.current_file):
            #     asset_filter = get_asset_texture_filtering(self.current_file)
            #     self.sprite_widget.set_asset_texture_filter(asset_filter)

    def _setup_panel(self):
        """Setup sprite animator panel UI"""
        main_widget = QWidget()
        main_layout = QVBoxLayout()
        main_layout.setContentsMargins(5, 5, 5, 5)

        # Toolbar
        toolbar = QHBoxLayout()

        new_btn = QPushButton("New")
        new_btn.clicked.connect(self._new_animation_file)
        toolbar.addWidget(new_btn)

        load_btn = QPushButton("Load")
        load_btn.clicked.connect(self._load_animation_file)
        toolbar.addWidget(load_btn)

        save_btn = QPushButton("Save")
        save_btn.clicked.connect(self._save_animation_file)
        toolbar.addWidget(save_btn)

        save_as_btn = QPushButton("Save As...")
        save_as_btn.clicked.connect(self._save_as_animation_file)
        toolbar.addWidget(save_as_btn)

        toolbar.addStretch()

        main_layout.addLayout(toolbar)

        # Main splitter (horizontal)
        main_splitter = QSplitter(Qt.Orientation.Horizontal)

        # Left side - Sprite sheet and animations
        # Use a vertical splitter to allow resizing between sprite source and animations
        left_splitter = QSplitter(Qt.Orientation.Vertical)
        left_splitter.setChildrenCollapsible(False)

        # Image loading section
        image_group = QGroupBox("Sprite Source")
        image_layout = QVBoxLayout()

        image_buttons = QHBoxLayout()
        load_sheet_btn = QPushButton("Load Sprite Sheet")
        load_sheet_btn.clicked.connect(self._load_sprite_sheet)
        image_buttons.addWidget(load_sheet_btn)

        load_images_btn = QPushButton("Load Images")
        load_images_btn.clicked.connect(self._load_images)
        image_buttons.addWidget(load_images_btn)

        image_layout.addLayout(image_buttons)

        # Aseprite import button
        aseprite_buttons = QHBoxLayout()
        load_aseprite_btn = QPushButton("Import Aseprite JSON")
        load_aseprite_btn.clicked.connect(self._load_aseprite_json)
        aseprite_buttons.addWidget(load_aseprite_btn)
        image_layout.addLayout(aseprite_buttons)

        # Sprite slice widget (scrollable)
        slice_scroll = QScrollArea()
        slice_scroll.setWidgetResizable(True)
        slice_scroll.setHorizontalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAlwaysOff)
        slice_scroll.setMinimumHeight(250)

        self.sprite_widget = SpriteSliceWidget()
        self.sprite_widget.slice_clicked.connect(self._on_sprite_clicked)
        slice_scroll.setWidget(self.sprite_widget)

        image_layout.addWidget(slice_scroll)

        # Slicing settings (below preview)
        slicing_group = QGroupBox("Slicing Settings")
        slicing_layout = QVBoxLayout()

        # Mode info label
        self.slice_mode_info = QLabel("Configure sprite sheet slicing")
        self.slice_mode_info.setStyleSheet("color: gray; font-style: italic;")
        slicing_layout.addWidget(self.slice_mode_info)

        # Mode selection
        mode_layout = QHBoxLayout()
        mode_label = QLabel("Mode:")
        mode_layout.addWidget(mode_label)

        self.slice_mode_combo = QComboBox()
        self.slice_mode_combo.addItems(["Grid (cols/rows)", "Size (width/height)"])
        self.slice_mode_combo.currentIndexChanged.connect(self._on_slice_mode_changed)
        mode_layout.addWidget(self.slice_mode_combo, 1)
        slicing_layout.addLayout(mode_layout)

        # Grid mode controls
        self.grid_controls = QWidget()
        grid_layout = QGridLayout()
        grid_layout.setContentsMargins(0, 0, 0, 0)

        grid_layout.addWidget(QLabel("Columns:"), 0, 0)
        self.cols_spin = QSpinBox()
        self.cols_spin.setMinimum(1)
        self.cols_spin.setMaximum(100)
        self.cols_spin.setValue(8)
        self.cols_spin.valueChanged.connect(self._on_slice_settings_changed)
        grid_layout.addWidget(self.cols_spin, 0, 1)

        grid_layout.addWidget(QLabel("Rows:"), 1, 0)
        self.rows_spin = QSpinBox()
        self.rows_spin.setMinimum(1)
        self.rows_spin.setMaximum(100)
        self.rows_spin.setValue(8)
        self.rows_spin.valueChanged.connect(self._on_slice_settings_changed)
        grid_layout.addWidget(self.rows_spin, 1, 1)

        self.grid_info_label = QLabel("Sprite size: N/A")
        self.grid_info_label.setStyleSheet("color: gray;")
        grid_layout.addWidget(self.grid_info_label, 2, 0, 1, 2)

        self.grid_controls.setLayout(grid_layout)
        slicing_layout.addWidget(self.grid_controls)

        # Size mode controls
        self.size_controls = QWidget()
        size_layout = QGridLayout()
        size_layout.setContentsMargins(0, 0, 0, 0)

        size_layout.addWidget(QLabel("Width:"), 0, 0)
        self.width_spin = QSpinBox()
        self.width_spin.setMinimum(1)
        self.width_spin.setMaximum(4096)
        self.width_spin.setValue(32)
        self.width_spin.valueChanged.connect(self._on_slice_settings_changed)
        size_layout.addWidget(self.width_spin, 0, 1)

        size_layout.addWidget(QLabel("Height:"), 1, 0)
        self.height_spin = QSpinBox()
        self.height_spin.setMinimum(1)
        self.height_spin.setMaximum(4096)
        self.height_spin.setValue(32)
        self.height_spin.valueChanged.connect(self._on_slice_settings_changed)
        size_layout.addWidget(self.height_spin, 1, 1)

        self.size_info_label = QLabel("Grid size: N/A")
        self.size_info_label.setStyleSheet("color: gray;")
        size_layout.addWidget(self.size_info_label, 2, 0, 1, 2)

        self.size_controls.setLayout(size_layout)
        self.size_controls.setVisible(False)
        slicing_layout.addWidget(self.size_controls)

        slicing_group.setLayout(slicing_layout)
        slicing_group.setEnabled(False)
        self.slicing_group = slicing_group
        image_layout.addWidget(slicing_group)

        image_group.setLayout(image_layout)
        left_splitter.addWidget(image_group)

        # Animation list section
        anim_group = QGroupBox("Animations")
        anim_layout = QVBoxLayout()

        anim_buttons = QHBoxLayout()
        new_anim_btn = QPushButton("New")
        new_anim_btn.clicked.connect(self._new_animation)
        anim_buttons.addWidget(new_anim_btn)

        rename_anim_btn = QPushButton("Rename")
        rename_anim_btn.clicked.connect(self._rename_animation)
        anim_buttons.addWidget(rename_anim_btn)

        delete_anim_btn = QPushButton("Delete")
        delete_anim_btn.clicked.connect(self._delete_animation)
        anim_buttons.addWidget(delete_anim_btn)

        anim_layout.addLayout(anim_buttons)

        self.animation_list = QListWidget()
        self.animation_list.currentItemChanged.connect(self._on_animation_selected)
        anim_layout.addWidget(self.animation_list)

        anim_group.setLayout(anim_layout)
        left_splitter.addWidget(anim_group)

        # Set initial sizes for the splitter (2:1 ratio for sprite source:animations)
        left_splitter.setSizes([400, 200])

        main_splitter.addWidget(left_splitter)

        # Right side - Animation details and preview
        right_widget = QWidget()
        right_layout = QVBoxLayout()
        right_layout.setContentsMargins(0, 0, 0, 0)

        # Animation settings
        settings_group = QGroupBox("Animation Settings")
        settings_layout = QFormLayout()

        self.loop_combo = QComboBox()
        self.loop_combo.addItems(["none", "linear", "pingpong"])
        self.loop_combo.currentTextChanged.connect(self._on_settings_changed)
        settings_layout.addRow("Loop:", self.loop_combo)

        self.speed_spin = QDoubleSpinBox()
        self.speed_spin.setMinimum(0.1)
        self.speed_spin.setMaximum(120.0)
        self.speed_spin.setValue(10.0)
        self.speed_spin.setSuffix(" FPS")
        self.speed_spin.valueChanged.connect(self._on_settings_changed)
        settings_layout.addRow("Speed:", self.speed_spin)

        settings_group.setLayout(settings_layout)
        right_layout.addWidget(settings_group)

        # Frame list
        frames_group = QGroupBox("Frames")
        frames_layout = QVBoxLayout()

        frames_label = QLabel("Click sprites on the left to add frames")
        frames_label.setWordWrap(True)
        frames_layout.addWidget(frames_label)

        self.frame_list = QListWidget()
        self.frame_list.setSelectionMode(QListWidget.SelectionMode.SingleSelection)
        frames_layout.addWidget(self.frame_list)

        frame_buttons = QHBoxLayout()
        delete_frame_btn = QPushButton("Delete Frame")
        delete_frame_btn.clicked.connect(self._delete_frame)
        frame_buttons.addWidget(delete_frame_btn)

        move_up_btn = QPushButton("Move Up")
        move_up_btn.clicked.connect(self._move_frame_up)
        frame_buttons.addWidget(move_up_btn)

        move_down_btn = QPushButton("Move Down")
        move_down_btn.clicked.connect(self._move_frame_down)
        frame_buttons.addWidget(move_down_btn)

        frames_layout.addLayout(frame_buttons)
        frames_group.setLayout(frames_layout)
        right_layout.addWidget(frames_group, 1)

        # Preview
        preview_group = QGroupBox("Preview")
        preview_layout = QVBoxLayout()

        self.preview_widget = AnimationPreviewWidget()
        self.preview_widget.set_sprite_widget(self.sprite_widget)
        preview_layout.addWidget(self.preview_widget, 1)

        preview_buttons = QHBoxLayout()
        play_btn = QPushButton("Play")
        play_btn.clicked.connect(self.preview_widget.play)
        preview_buttons.addWidget(play_btn)

        pause_btn = QPushButton("Pause")
        pause_btn.clicked.connect(self.preview_widget.pause)
        preview_buttons.addWidget(pause_btn)

        stop_btn = QPushButton("Stop")
        stop_btn.clicked.connect(self.preview_widget.stop)
        preview_buttons.addWidget(stop_btn)

        preview_layout.addLayout(preview_buttons)
        preview_group.setLayout(preview_layout)
        right_layout.addWidget(preview_group, 1)

        right_widget.setLayout(right_layout)
        main_splitter.addWidget(right_widget)

        main_splitter.setSizes([500, 400])
        main_layout.addWidget(main_splitter, 1)

        main_widget.setLayout(main_layout)
        self.content_layout.addWidget(main_widget)

    def _on_slice_mode_changed(self):
        """Handle slice mode change"""
        is_grid = self.slice_mode_combo.currentIndex() == 0
        self.grid_controls.setVisible(is_grid)
        self.size_controls.setVisible(not is_grid)
        self._on_slice_settings_changed()

    def _on_slice_settings_changed(self):
        """Handle slice settings change - re-slice the current sheet"""
        is_grid = self.slice_mode_combo.currentIndex() == 0

        # Handle batch images display grid
        if not self.current_sheet_path and self.loaded_image_paths:
            cols = self.cols_spin.value() if is_grid else 8
            self.sprite_widget.set_display_grid(cols)
            self.sprite_config['display_cols'] = cols
            return

        # Handle sprite sheet
        if not self.current_sheet_path:
            return

        pixmap = QPixmap(str(self.current_sheet_path))
        if pixmap.isNull():
            return

        if is_grid:
            cols = self.cols_spin.value()
            rows = self.rows_spin.value()
            w = pixmap.width() // cols
            h = pixmap.height() // rows
            self.grid_info_label.setText(f"Sprite size: {w} x {h} px ({cols * rows} sprites)")

            self.sprite_widget.load_sprite_sheet(self.current_sheet_path, cols, rows)
            self.sprite_widget.set_display_grid(cols, rows)
            self.sprite_config = {'type': 'sheet_grid', 'cols': cols, 'rows': rows}
        else:
            w = self.width_spin.value()
            h = self.height_spin.value()
            cols = pixmap.width() // w
            rows = pixmap.height() // h
            self.size_info_label.setText(f"Grid size: {cols} x {rows} ({cols * rows} sprites)")

            self.sprite_widget.load_sprite_sheet_by_size(self.current_sheet_path, w, h)
            self.sprite_widget.set_display_grid(cols, rows)
            self.sprite_config = {'type': 'sheet_size', 'width': w, 'height': h}

        # Reapply texture filtering after re-slicing
        self._apply_texture_filtering()

    def _new_animation_file(self):
        """Create a new animation file"""
        if self.animations and self.current_file:
            reply = QMessageBox.question(
                self,
                "Unsaved Changes",
                "Create new file? Unsaved changes will be lost.",
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
            )
            if reply == QMessageBox.StandardButton.No:
                return

        self.animations.clear()
        self.selected_animation = None
        self.loaded_image_paths.clear()
        self.current_file = None
        self.current_sheet_path = None
        self.sprite_config = {}
        self.asset_uuid = str(uuid.uuid4())

        self.animation_list.clear()
        self.frame_list.clear()
        self.sprite_widget.sprites.clear()
        self.sprite_widget.images.clear()
        self.sprite_widget.update()
        self.slicing_group.setEnabled(False)
        self.slice_mode_info.setText("Configure sprite sheet slicing")

    def _load_animation_file(self):
        """Load a .spriteanimation file"""
        file_path, _ = QFileDialog.getOpenFileName(
            self,
            "Load Sprite Animation",
            str(self._get_animations_directory()) if self.project_root else "",
            "Sprite Animation (*.spriteanimation)"
        )

        if file_path:
            try:
                with open(file_path, 'r', encoding='utf-8') as f:
                    data = json.load(f)

                self.current_file = file_path
                self.asset_uuid = data.get('uuid', str(uuid.uuid4()))
                self.loaded_image_paths = data.get('image_paths', [])

                # Load sprites
                sprite_config = data.get('sprite_config', {})
                self.sprite_config = sprite_config

                if sprite_config.get('type') == 'images':
                    self.sprite_widget.load_images(self.loaded_image_paths)
                    display_cols = sprite_config.get('display_cols', 8)
                    self.sprite_widget.set_display_grid(display_cols)

                    # Enable grid control for images
                    self.slicing_group.setEnabled(True)
                    self.slice_mode_info.setText("Configure display grid for image batch")
                    self.size_controls.setVisible(False)
                    self.cols_spin.blockSignals(True)
                    self.cols_spin.setValue(display_cols)
                    self.cols_spin.blockSignals(False)
                elif sprite_config.get('type') == 'sheet_grid':
                    self.current_sheet_path = self.loaded_image_paths[0]

                    # Block signals to prevent multiple re-slicing during load
                    self.cols_spin.blockSignals(True)
                    self.rows_spin.blockSignals(True)
                    self.slice_mode_combo.blockSignals(True)

                    self.cols_spin.setValue(sprite_config['cols'])
                    self.rows_spin.setValue(sprite_config['rows'])
                    self.slice_mode_combo.setCurrentIndex(0)

                    self.cols_spin.blockSignals(False)
                    self.rows_spin.blockSignals(False)
                    self.slice_mode_combo.blockSignals(False)

                    self.slicing_group.setEnabled(True)
                    self.slice_mode_info.setText("Configure sprite sheet slicing")
                    # Now slice with the loaded settings
                    self._on_slice_settings_changed()
                elif sprite_config.get('type') == 'sheet_size':
                    self.current_sheet_path = self.loaded_image_paths[0]

                    # Block signals to prevent multiple re-slicing during load
                    self.width_spin.blockSignals(True)
                    self.height_spin.blockSignals(True)
                    self.slice_mode_combo.blockSignals(True)

                    self.width_spin.setValue(sprite_config['width'])
                    self.height_spin.setValue(sprite_config['height'])
                    self.slice_mode_combo.setCurrentIndex(1)

                    self.width_spin.blockSignals(False)
                    self.height_spin.blockSignals(False)
                    self.slice_mode_combo.blockSignals(False)

                    self.slicing_group.setEnabled(True)
                    self.slice_mode_info.setText("Configure sprite sheet slicing")
                    # Now slice with the loaded settings
                    self._on_slice_settings_changed()

                # Load animations
                self.animations.clear()
                for anim_data in data.get('animations', []):
                    anim = Animation(anim_data['name'])
                    anim.frames = anim_data['frames']
                    anim.loop_behavior = anim_data.get('loop_behavior', 'linear')
                    anim.speed = anim_data.get('speed', 10.0)
                    self.animations[anim.name] = anim

                self._refresh_animation_list()
                self._apply_texture_filtering()

                QMessageBox.information(self, "Success", "Animation file loaded successfully")
            except Exception as e:
                QMessageBox.warning(self, "Error", f"Failed to load animation file:\n{str(e)}")

    def _save_animation_file(self):
        """Save the current animation file"""
        if not self.current_file:
            self._save_as_animation_file()
            return

        self._do_save(self.current_file)

    def _save_as_animation_file(self):
        """Save animation file with a new name"""
        file_path, _ = QFileDialog.getSaveFileName(
            self,
            "Save Sprite Animation",
            str(self._get_animations_directory()) if self.project_root else "",
            "Sprite Animation (*.spriteanimation)"
        )

        if file_path:
            if not file_path.endswith('.spriteanimation'):
                file_path += '.spriteanimation'

            self.current_file = file_path
            self._do_save(file_path)

    def _do_save(self, file_path):
        """Perform the save operation"""
        try:
            # Build animation data
            animations_data = []
            for name, anim in self.animations.items():
                animations_data.append({
                    'name': anim.name,
                    'frames': anim.frames,
                    'loop_behavior': anim.loop_behavior,
                    'speed': anim.speed
                })

            data = {
                'uuid': self.asset_uuid,
                'image_paths': self.loaded_image_paths,
                'sprite_config': self.sprite_config,
                'animations': animations_data
            }

            with open(file_path, 'w', encoding='utf-8') as f:
                json.dump(data, f, indent=2)

            QMessageBox.information(self, "Success", "Animation file saved successfully")
        except Exception as e:
            QMessageBox.warning(self, "Error", f"Failed to save animation file:\n{str(e)}")

    def _load_sprite_sheet(self):
        """Load a sprite sheet image"""
        file_path, _ = QFileDialog.getOpenFileName(
            self,
            "Load Sprite Sheet",
            str(self.project_root) if self.project_root else "",
            "Images (*.png *.jpg *.jpeg *.bmp)"
        )

        if file_path:
            self.current_sheet_path = file_path
            self.loaded_image_paths = [file_path]
            self.slicing_group.setEnabled(True)
            self.slice_mode_info.setText("Configure sprite sheet slicing")

            # Apply current slicing settings
            self._on_slice_settings_changed()

            # Apply texture filtering to newly loaded sprites
            self._apply_texture_filtering()

            # Create default animation
            self._create_default_animation()

    def _load_images(self):
        """Load multiple images as individual sprites"""
        file_paths, _ = QFileDialog.getOpenFileNames(
            self,
            "Load Images",
            str(self.project_root) if self.project_root else "",
            "Images (*.png *.jpg *.jpeg *.bmp)"
        )

        if file_paths:
            self.loaded_image_paths = file_paths
            self.current_sheet_path = None
            self.sprite_widget.load_images(file_paths)

            # Enable slicing settings for batch images (controls display grid)
            self.slicing_group.setEnabled(True)
            self.slice_mode_info.setText("Configure display grid for image batch")
            self.size_controls.setVisible(False)

            # Set initial display grid
            cols = self.cols_spin.value()
            self.sprite_widget.set_display_grid(cols)
            self.sprite_config = {'type': 'images', 'display_cols': cols}

            # Apply texture filtering to newly loaded sprites
            self._apply_texture_filtering()

            # Create default animation
            self._create_default_animation()

    def _load_aseprite_json(self):
        """Load Aseprite JSON export"""
        file_path, _ = QFileDialog.getOpenFileName(
            self,
            "Load Aseprite JSON",
            str(self.project_root) if self.project_root else "",
            "JSON Files (*.json)"
        )

        if not file_path:
            return

        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                data = json.load(f)

            # Parse Aseprite JSON
            frames_data = data.get('frames', {})
            meta = data.get('meta', {})

            # Get the image path (relative to JSON file)
            json_dir = Path(file_path).parent
            image_name = meta.get('image', '')
            if not image_name:
                QMessageBox.warning(self, "Error", "No image path found in Aseprite JSON")
                return

            image_path = json_dir / image_name
            if not image_path.exists():
                QMessageBox.warning(self, "Error", f"Image file not found: {image_path}")
                return

            # Load the sprite sheet
            pixmap = QPixmap(str(image_path))
            if pixmap.isNull():
                QMessageBox.warning(self, "Error", f"Failed to load image: {image_path}")
                return

            self.current_sheet_path = str(image_path)
            self.loaded_image_paths = [str(image_path)]
            self.is_aseprite_import = True

            # Parse frames (handle both array and object formats)
            if isinstance(frames_data, list):
                # Array format
                frame_list = frames_data
            else:
                # Object format
                frame_list = list(frames_data.values())

            # Create sprite slices from frames
            self.sprite_widget.sprites.clear()
            self.sprite_widget.images = [pixmap]

            for idx, frame_data in enumerate(frame_list):
                frame_rect = frame_data.get('frame', {})
                x = frame_rect.get('x', 0)
                y = frame_rect.get('y', 0)
                w = frame_rect.get('w', 0)
                h = frame_rect.get('h', 0)

                rect = QRect(x, y, w, h)
                self.sprite_widget.sprites.append(SpriteSlice(rect, idx, str(image_path)))

            # Calculate display grid based on sprite count
            sprite_count = len(self.sprite_widget.sprites)
            cols = min(8, sprite_count)
            rows = (sprite_count + cols - 1) // cols
            self.sprite_widget.set_display_grid(cols, rows)

            self.sprite_config = {
                'type': 'aseprite',
                'json_path': file_path,
                'image_path': str(image_path)
            }

            # Disable slicing controls for Aseprite imports
            self.slicing_group.setEnabled(False)
            self.slice_mode_info.setText(f"Imported from Aseprite ({len(frame_list)} frames)")

            self.sprite_widget.update()

            # Apply texture filtering
            self._apply_texture_filtering()

            # Parse animations from tags if present
            self.animations.clear()
            tags = meta.get('frameTags', [])

            if tags:
                for tag in tags:
                    tag_name = tag.get('name', 'unnamed')
                    from_frame = tag.get('from', 0)
                    to_frame = tag.get('to', len(frame_list) - 1)
                    direction = tag.get('direction', 'forward')

                    anim = Animation(tag_name)
                    anim.frames = list(range(from_frame, to_frame + 1))

                    # Map Aseprite direction to loop behavior
                    if direction == 'pingpong':
                        anim.loop_behavior = 'pingpong'
                    elif direction == 'forward':
                        anim.loop_behavior = 'linear'
                    else:
                        anim.loop_behavior = 'none'

                    # Try to get speed from frame durations
                    if frame_list:
                        avg_duration = sum(f.get('duration', 100) for f in frame_list) / len(frame_list)
                        anim.speed = 1000.0 / avg_duration if avg_duration > 0 else 10.0

                    self.animations[tag_name] = anim

                self._refresh_animation_list()

                # Select first animation
                if self.animation_list.count() > 0:
                    self.animation_list.setCurrentRow(0)
            else:
                # No tags, create default animation
                self._create_default_animation()

            QMessageBox.information(self, "Success",
                                  f"Imported {len(frame_list)} frames from Aseprite\n"
                                  f"{len(tags)} animations found")

        except Exception as e:
            QMessageBox.warning(self, "Error", f"Failed to load Aseprite JSON:\n{str(e)}")
            import traceback
            traceback.print_exc()

    def _create_default_animation(self):
        """Create a default animation with all sprites"""
        if self.sprite_widget.get_sprite_count() == 0:
            return

        # Check if 'default' animation already exists
        if 'default' not in self.animations:
            anim = Animation('default')
            anim.loop_behavior = 'linear'
            anim.speed = 10.0
            # Add all sprites as frames
            anim.frames = list(range(self.sprite_widget.get_sprite_count()))
            self.animations['default'] = anim

            self._refresh_animation_list()

            # Select the default animation
            for i in range(self.animation_list.count()):
                if self.animation_list.item(i).text() == 'default':
                    self.animation_list.setCurrentRow(i)
                    break

    def _new_animation(self):
        """Create a new animation"""
        dialog = NewAnimationDialog(self)
        if dialog.exec() == QDialog.DialogCode.Accepted:
            data = dialog.get_animation_data()

            # Check for duplicate name
            if data['name'] in self.animations:
                QMessageBox.warning(self, "Error", "Animation with this name already exists")
                return

            anim = Animation(data['name'])
            anim.loop_behavior = data['loop_behavior']
            anim.speed = data['speed']
            self.animations[data['name']] = anim

            self._refresh_animation_list()

            # Select the new animation
            for i in range(self.animation_list.count()):
                if self.animation_list.item(i).text() == data['name']:
                    self.animation_list.setCurrentRow(i)
                    break

    def _rename_animation(self):
        """Rename the selected animation"""
        if not self.selected_animation:
            QMessageBox.warning(self, "Error", "No animation selected")
            return

        from PyQt6.QtWidgets import QInputDialog
        new_name, ok = QInputDialog.getText(
            self, "Rename Animation", "New name:",
            QLineEdit.EchoMode.Normal, self.selected_animation.name
        )

        if ok and new_name:
            if new_name in self.animations and new_name != self.selected_animation.name:
                QMessageBox.warning(self, "Error", "Animation with this name already exists")
                return

            old_name = self.selected_animation.name
            self.animations[new_name] = self.animations.pop(old_name)
            self.selected_animation.name = new_name

            self._refresh_animation_list()

    def _delete_animation(self):
        """Delete the selected animation"""
        if not self.selected_animation:
            QMessageBox.warning(self, "Error", "No animation selected")
            return

        reply = QMessageBox.question(
            self,
            "Delete Animation",
            f"Delete animation '{self.selected_animation.name}'?",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
        )

        if reply == QMessageBox.StandardButton.Yes:
            del self.animations[self.selected_animation.name]
            self.selected_animation = None
            self._refresh_animation_list()

    def _on_animation_selected(self, current, previous):
        """Handle animation selection"""
        if current:
            anim_name = current.text()
            self.selected_animation = self.animations.get(anim_name)

            if self.selected_animation:
                # Update settings
                self.loop_combo.blockSignals(True)
                self.speed_spin.blockSignals(True)

                self.loop_combo.setCurrentText(self.selected_animation.loop_behavior)
                self.speed_spin.setValue(self.selected_animation.speed)

                self.loop_combo.blockSignals(False)
                self.speed_spin.blockSignals(False)

                # Update frame list
                self._refresh_frame_list()

                # Update preview
                self.preview_widget.set_animation(self.selected_animation)
        else:
            self.selected_animation = None
            self.frame_list.clear()

    def _on_settings_changed(self):
        """Handle animation settings change"""
        if self.selected_animation:
            self.selected_animation.loop_behavior = self.loop_combo.currentText()
            self.selected_animation.speed = self.speed_spin.value()

            # Update preview
            self.preview_widget.set_animation(self.selected_animation)

    def _on_sprite_clicked(self, sprite_index):
        """Handle sprite click to add frame"""
        if not self.selected_animation:
            QMessageBox.warning(self, "Error", "No animation selected. Create or select an animation first.")
            return

        self.selected_animation.frames.append(sprite_index)
        self._refresh_frame_list()

        # Update preview
        self.preview_widget.set_animation(self.selected_animation)

    def _delete_frame(self):
        """Delete the selected frame"""
        if not self.selected_animation:
            return

        current_row = self.frame_list.currentRow()
        if current_row >= 0:
            del self.selected_animation.frames[current_row]
            self._refresh_frame_list()

            # Update preview
            self.preview_widget.set_animation(self.selected_animation)

    def _move_frame_up(self):
        """Move selected frame up"""
        if not self.selected_animation:
            return

        current_row = self.frame_list.currentRow()
        if current_row > 0:
            frames = self.selected_animation.frames
            frames[current_row], frames[current_row - 1] = frames[current_row - 1], frames[current_row]
            self._refresh_frame_list()
            self.frame_list.setCurrentRow(current_row - 1)

    def _move_frame_down(self):
        """Move selected frame down"""
        if not self.selected_animation:
            return

        current_row = self.frame_list.currentRow()
        frames = self.selected_animation.frames
        if current_row >= 0 and current_row < len(frames) - 1:
            frames[current_row], frames[current_row + 1] = frames[current_row + 1], frames[current_row]
            self._refresh_frame_list()
            self.frame_list.setCurrentRow(current_row + 1)

    def _refresh_animation_list(self):
        """Refresh the animation list widget"""
        self.animation_list.clear()
        for name in sorted(self.animations.keys()):
            self.animation_list.addItem(name)

    def _refresh_frame_list(self):
        """Refresh the frame list widget"""
        self.frame_list.clear()
        if self.selected_animation:
            for idx, sprite_idx in enumerate(self.selected_animation.frames):
                self.frame_list.addItem(f"Frame {idx}: Sprite {sprite_idx}")

    def _get_animations_directory(self):
        """Get the animations directory path"""
        if self.project_root:
            anim_dir = Path(self.project_root) / "assets" / "animations"
            anim_dir.mkdir(parents=True, exist_ok=True)
            return anim_dir
        return Path.home()
