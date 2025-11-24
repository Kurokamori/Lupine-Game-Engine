"""
TileMap 2D Editor Tool
A comprehensive tool for creating and editing 2D tilemaps with multiple layers
"""

from PyQt6.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QLabel,
                             QPushButton, QListWidget, QListWidgetItem,
                             QFileDialog, QMessageBox, QDialog, QFormLayout,
                             QLineEdit, QComboBox, QDialogButtonBox, QSpinBox,
                             QDoubleSpinBox, QSplitter, QGroupBox, QGridLayout,
                             QScrollArea, QRadioButton, QButtonGroup, QFrame,
                             QCheckBox, QTextEdit, QTabWidget, QColorDialog,
                             QSlider, QToolButton, QMenu)
from PyQt6.QtCore import Qt, QTimer, pyqtSignal, QSize, QRect, QPoint, QPointF
from PyQt6.QtGui import QPixmap, QImage, QPainter, QColor, QPen, QPolygonF, QBrush, QWheelEvent, QMouseEvent, QPainterPath
from pathlib import Path
import json
import sys
import uuid
import copy

# Import base panel
editor_path = Path(__file__).parent.parent.parent
if str(editor_path) not in sys.path:
    sys.path.insert(0, str(editor_path))

from panels.base_panel import EditorPanel


class TileLayer:
    """Represents a single tile layer"""
    def __init__(self, name: str, width: int, height: int):
        self.name = name
        self.width = width
        self.height = height
        self.visible = True
        self.opacity = 1.0
        self.offset_x = 0  # Grid offset in tiles
        self.offset_y = 0  # Grid offset in tiles
        # Store tiles as dict: {(x, y): {'tileset_index': int, 'tile_index': int}}
        self.tiles = {}

    def set_tile(self, x: int, y: int, tileset_index: int, tile_index: int):
        """Set a tile at position"""
        if 0 <= x < self.width and 0 <= y < self.height:
            self.tiles[(x, y)] = {'tileset_index': tileset_index, 'tile_index': tile_index}

    def get_tile(self, x: int, y: int):
        """Get tile at position"""
        return self.tiles.get((x, y))

    def clear_tile(self, x: int, y: int):
        """Clear tile at position"""
        if (x, y) in self.tiles:
            del self.tiles[(x, y)]

    def clear_all(self):
        """Clear all tiles"""
        self.tiles.clear()


class TilesetData:
    """Represents a loaded tileset"""
    def __init__(self, file_path: str):
        self.file_path = file_path
        self.name = Path(file_path).stem
        self.image_pixmap = None
        self.tile_width = 32
        self.tile_height = 32
        self.tiles = []  # List of tile metadata

    def load(self):
        """Load tileset from file"""
        try:
            with open(self.file_path, 'r', encoding='utf-8') as f:
                data = json.load(f)

            self.tile_width = data.get('tile_width', 32)
            self.tile_height = data.get('tile_height', 32)
            tileset_image_path = data.get('tileset_image_path', '')

            if tileset_image_path and Path(tileset_image_path).exists():
                self.image_pixmap = QPixmap(str(tileset_image_path))

                # Calculate tile count
                cols = self.image_pixmap.width() // self.tile_width
                rows = self.image_pixmap.height() // self.tile_height

                # Store tiles metadata
                self.tiles = data.get('tiles', [])

                return True
        except Exception as e:
            print(f"Failed to load tileset: {e}")
            return False

        return False

    def get_tile_count(self):
        """Get number of tiles"""
        if not self.image_pixmap:
            return 0
        cols = self.image_pixmap.width() // self.tile_width
        rows = self.image_pixmap.height() // self.tile_height
        return cols * rows

    def get_tile_pixmap(self, tile_index: int):
        """Get pixmap for a specific tile"""
        if not self.image_pixmap:
            return QPixmap()

        cols = self.image_pixmap.width() // self.tile_width
        row = tile_index // cols
        col = tile_index % cols

        x = col * self.tile_width
        y = row * self.tile_height

        return self.image_pixmap.copy(x, y, self.tile_width, self.tile_height)


class TilePaletteWidget(QWidget):
    """Widget for displaying and selecting tiles from all loaded tilesets"""

    tile_selected = pyqtSignal(int, int)  # tileset_index, tile_index

    def __init__(self, parent=None):
        super().__init__(parent)
        self.tilesets = []  # List of TilesetData
        self.selected_tileset_index = -1
        self.selected_tile_index = -1
        self.hover_tileset_index = -1
        self.hover_tile_index = -1
        self.display_scale = 2.0
        self.transformation_mode = Qt.TransformationMode.SmoothTransformation

        self.setMinimumSize(200, 300)
        self.setMouseTracking(True)

    def set_texture_filtering(self, filter_mode: str):
        """Set texture filtering mode"""
        filter_map = {
            "nearest": Qt.TransformationMode.FastTransformation,
            "bilinear": Qt.TransformationMode.SmoothTransformation,
            "cubic": Qt.TransformationMode.SmoothTransformation
        }
        self.transformation_mode = filter_map.get(filter_mode.lower(), Qt.TransformationMode.SmoothTransformation)
        self.update()

    def add_tileset(self, tileset: TilesetData):
        """Add a tileset to the palette"""
        self.tilesets.append(tileset)
        self.update()

    def clear_tilesets(self):
        """Clear all tilesets"""
        self.tilesets.clear()
        self.selected_tileset_index = -1
        self.selected_tile_index = -1
        self.update()

    def paintEvent(self, event):
        """Draw all tiles from all tilesets"""
        painter = QPainter(self)
        painter.fillRect(self.rect(), QColor(40, 40, 40))

        if not self.tilesets:
            painter.setPen(QColor(200, 200, 200))
            painter.drawText(self.rect(), Qt.AlignmentFlag.AlignCenter,
                           "Load a tileset")
            return

        y_offset = 5
        padding = 4

        for tileset_idx, tileset in enumerate(self.tilesets):
            if not tileset.image_pixmap:
                continue

            # Draw tileset name header
            painter.setPen(QColor(200, 200, 200))
            painter.drawText(5, y_offset + 15, f"{tileset.name}")
            y_offset += 25

            # Calculate grid
            tile_count = tileset.get_tile_count()
            cols = tileset.image_pixmap.width() // tileset.tile_width
            display_tile_width = int(tileset.tile_width * self.display_scale)
            display_tile_height = int(tileset.tile_height * self.display_scale)

            # Calculate how many columns fit in widget width
            available_width = self.width() - padding * 2
            tiles_per_row = max(1, available_width // (display_tile_width + padding))

            # Draw tiles
            for tile_idx in range(tile_count):
                row = tile_idx // tiles_per_row
                col = tile_idx % tiles_per_row

                x = col * (display_tile_width + padding) + padding
                y = y_offset + row * (display_tile_height + padding)

                # Draw tile
                tile_pixmap = tileset.get_tile_pixmap(tile_idx)
                if not tile_pixmap.isNull():
                    scaled = tile_pixmap.scaled(display_tile_width, display_tile_height,
                                              Qt.AspectRatioMode.IgnoreAspectRatio,
                                              self.transformation_mode)
                    painter.drawPixmap(x, y, scaled)

                # Draw selection/hover border
                if tileset_idx == self.selected_tileset_index and tile_idx == self.selected_tile_index:
                    painter.setPen(QPen(QColor(0, 120, 215), 3))
                    painter.drawRect(x - 2, y - 2, display_tile_width + 4, display_tile_height + 4)
                elif tileset_idx == self.hover_tileset_index and tile_idx == self.hover_tile_index:
                    painter.setPen(QPen(QColor(100, 100, 100), 2))
                    painter.drawRect(x - 1, y - 1, display_tile_width + 2, display_tile_height + 2)

            # Calculate height of this tileset section
            rows = (tile_count + tiles_per_row - 1) // tiles_per_row
            y_offset += rows * (display_tile_height + padding) + padding + 10

        # Update minimum height
        self.setMinimumHeight(y_offset)

    def mousePressEvent(self, event):
        """Handle tile selection"""
        if event.button() == Qt.MouseButton.LeftButton:
            tileset_idx, tile_idx = self._get_tile_at_pos(event.pos())
            if tileset_idx >= 0 and tile_idx >= 0:
                self.selected_tileset_index = tileset_idx
                self.selected_tile_index = tile_idx
                self.tile_selected.emit(tileset_idx, tile_idx)
                self.update()

    def mouseMoveEvent(self, event):
        """Handle hover"""
        old_hover_tileset = self.hover_tileset_index
        old_hover_tile = self.hover_tile_index
        self.hover_tileset_index, self.hover_tile_index = self._get_tile_at_pos(event.pos())
        if old_hover_tileset != self.hover_tileset_index or old_hover_tile != self.hover_tile_index:
            self.update()

    def _get_tile_at_pos(self, pos):
        """Get tileset and tile index at position"""
        y_offset = 5
        padding = 4

        for tileset_idx, tileset in enumerate(self.tilesets):
            if not tileset.image_pixmap:
                continue

            # Skip header
            y_offset += 25

            # Calculate grid
            tile_count = tileset.get_tile_count()
            display_tile_width = int(tileset.tile_width * self.display_scale)
            display_tile_height = int(tileset.tile_height * self.display_scale)

            available_width = self.width() - padding * 2
            tiles_per_row = max(1, available_width // (display_tile_width + padding))

            # Check if pos is in this tileset area
            rows = (tile_count + tiles_per_row - 1) // tiles_per_row
            tileset_height = rows * (display_tile_height + padding)

            if y_offset <= pos.y() <= y_offset + tileset_height:
                # Check which tile
                for tile_idx in range(tile_count):
                    row = tile_idx // tiles_per_row
                    col = tile_idx % tiles_per_row

                    x = col * (display_tile_width + padding) + padding
                    y = y_offset + row * (display_tile_height + padding)

                    if (x <= pos.x() <= x + display_tile_width and
                        y <= pos.y() <= y + display_tile_height):
                        return tileset_idx, tile_idx

            y_offset += tileset_height + padding + 10

        return -1, -1


class TileMapCanvas(QWidget):
    """Widget for drawing and editing the tilemap"""

    tile_painted = pyqtSignal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self.layers = []  # List of TileLayer
        self.current_layer_index = 0
        self.tilesets = []  # Reference to tilesets
        self.tile_width = 32
        self.tile_height = 32
        self.grid_visible = True
        self.display_scale = 1.0
        self.transformation_mode = Qt.TransformationMode.SmoothTransformation

        # Paint tool settings
        self.selected_tileset_index = -1
        self.selected_tile_index = -1
        self.tool_mode = "paint"  # paint, erase, fill, rectangle
        self.is_painting = False

        # Rectangle tool state
        self.rect_start_pos = None
        self.rect_current_pos = None

        # Camera/view
        self.camera_x = 0
        self.camera_y = 0

        self.setMinimumSize(600, 400)
        self.setMouseTracking(True)
        self.setFocusPolicy(Qt.FocusPolicy.StrongFocus)

    def set_texture_filtering(self, filter_mode: str):
        """Set texture filtering mode"""
        filter_map = {
            "nearest": Qt.TransformationMode.FastTransformation,
            "bilinear": Qt.TransformationMode.SmoothTransformation,
            "cubic": Qt.TransformationMode.SmoothTransformation
        }
        self.transformation_mode = filter_map.get(filter_mode.lower(), Qt.TransformationMode.SmoothTransformation)
        self.update()

    def add_layer(self, layer: TileLayer):
        """Add a layer"""
        self.layers.append(layer)
        self.update()

    def get_current_layer(self):
        """Get current layer"""
        if 0 <= self.current_layer_index < len(self.layers):
            return self.layers[self.current_layer_index]
        return None

    def paintEvent(self, event):
        """Draw the tilemap"""
        painter = QPainter(self)
        painter.fillRect(self.rect(), QColor(50, 50, 50))

        if not self.layers:
            painter.setPen(QColor(200, 200, 200))
            painter.drawText(self.rect(), Qt.AlignmentFlag.AlignCenter,
                           "Create a layer to start painting")
            return

        # Draw all layers
        for layer in self.layers:
            if not layer.visible:
                continue

            # Set layer opacity
            painter.setOpacity(layer.opacity)

            # Draw tiles in this layer
            for (tile_x, tile_y), tile_data in layer.tiles.items():
                tileset_idx = tile_data['tileset_index']
                tile_idx = tile_data['tile_index']

                if tileset_idx < 0 or tileset_idx >= len(self.tilesets):
                    continue

                tileset = self.tilesets[tileset_idx]
                tile_pixmap = tileset.get_tile_pixmap(tile_idx)

                if tile_pixmap.isNull():
                    continue

                # Calculate screen position with layer offset
                actual_x = tile_x + layer.offset_x
                actual_y = tile_y + layer.offset_y

                screen_x = int((actual_x * self.tile_width - self.camera_x) * self.display_scale)
                screen_y = int((actual_y * self.tile_height - self.camera_y) * self.display_scale)

                display_width = int(self.tile_width * self.display_scale)
                display_height = int(self.tile_height * self.display_scale)

                # Only draw if visible on screen
                if (screen_x + display_width >= 0 and screen_x < self.width() and
                    screen_y + display_height >= 0 and screen_y < self.height()):

                    scaled = tile_pixmap.scaled(display_width, display_height,
                                              Qt.AspectRatioMode.IgnoreAspectRatio,
                                              self.transformation_mode)
                    painter.drawPixmap(screen_x, screen_y, scaled)

        # Reset opacity
        painter.setOpacity(1.0)

        # Draw grid
        if self.grid_visible:
            layer = self.get_current_layer()
            if layer:
                painter.setPen(QPen(QColor(100, 100, 100, 100), 1))

                display_tile_width = int(self.tile_width * self.display_scale)
                display_tile_height = int(self.tile_height * self.display_scale)

                # Calculate grid offset based on layer offset
                grid_offset_x = int((layer.offset_x * self.tile_width - self.camera_x) * self.display_scale)
                grid_offset_y = int((layer.offset_y * self.tile_height - self.camera_y) * self.display_scale)

                # Draw vertical lines
                for x in range(layer.width + 1):
                    screen_x = grid_offset_x + x * display_tile_width
                    if 0 <= screen_x <= self.width():
                        painter.drawLine(screen_x, 0, screen_x, self.height())

                # Draw horizontal lines
                for y in range(layer.height + 1):
                    screen_y = grid_offset_y + y * display_tile_height
                    if 0 <= screen_y <= self.height():
                        painter.drawLine(0, screen_y, self.width(), screen_y)

        # Draw rectangle preview when using rectangle tool
        if self.tool_mode == "rectangle" and self.rect_start_pos and self.rect_current_pos:
            layer = self.get_current_layer()
            if layer:
                painter.setPen(QPen(QColor(0, 120, 215), 2))
                painter.setBrush(QBrush(QColor(0, 120, 215, 50)))

                display_tile_width = int(self.tile_width * self.display_scale)
                display_tile_height = int(self.tile_height * self.display_scale)

                grid_offset_x = int((layer.offset_x * self.tile_width - self.camera_x) * self.display_scale)
                grid_offset_y = int((layer.offset_y * self.tile_height - self.camera_y) * self.display_scale)

                # Convert tile coords to screen coords
                start_screen_x = grid_offset_x + self.rect_start_pos[0] * display_tile_width
                start_screen_y = grid_offset_y + self.rect_start_pos[1] * display_tile_height
                end_screen_x = grid_offset_x + (self.rect_current_pos[0] + 1) * display_tile_width
                end_screen_y = grid_offset_y + (self.rect_current_pos[1] + 1) * display_tile_height

                rect_x = min(start_screen_x, end_screen_x)
                rect_y = min(start_screen_y, end_screen_y)
                rect_w = abs(end_screen_x - start_screen_x)
                rect_h = abs(end_screen_y - start_screen_y)

                painter.drawRect(rect_x, rect_y, rect_w, rect_h)

    def mousePressEvent(self, event):
        """Handle mouse press for painting"""
        if event.button() == Qt.MouseButton.LeftButton:
            if self.tool_mode == "rectangle":
                # Start rectangle
                tile_pos = self._get_tile_at_screen_pos(event.pos())
                if tile_pos:
                    self.rect_start_pos = tile_pos
                    self.rect_current_pos = tile_pos
                    self.update()
            elif self.tool_mode == "fill":
                # Fill bucket
                self._fill_at_pos(event.pos())
            else:
                # Paint or erase
                self.is_painting = True
                self._paint_at_pos(event.pos())
        elif event.button() == Qt.MouseButton.MiddleButton:
            self.last_pan_pos = event.pos()

    def mouseMoveEvent(self, event):
        """Handle mouse move for painting"""
        if self.tool_mode == "rectangle" and self.rect_start_pos:
            # Update rectangle preview
            tile_pos = self._get_tile_at_screen_pos(event.pos())
            if tile_pos:
                self.rect_current_pos = tile_pos
                self.update()
        elif self.is_painting:
            self._paint_at_pos(event.pos())
        elif event.buttons() & Qt.MouseButton.MiddleButton:
            # Pan camera
            delta = event.pos() - self.last_pan_pos
            self.camera_x -= delta.x() / self.display_scale
            self.camera_y -= delta.y() / self.display_scale
            self.last_pan_pos = event.pos()
            self.update()

    def mouseReleaseEvent(self, event):
        """Handle mouse release"""
        if event.button() == Qt.MouseButton.LeftButton:
            if self.tool_mode == "rectangle" and self.rect_start_pos and self.rect_current_pos:
                # Complete rectangle
                self._complete_rectangle()
                self.rect_start_pos = None
                self.rect_current_pos = None
                self.update()
            else:
                self.is_painting = False

    def wheelEvent(self, event: QWheelEvent):
        """Handle zoom with mouse wheel"""
        delta = event.angleDelta().y()

        if delta > 0:
            self.display_scale = min(8.0, self.display_scale * 1.1)
        else:
            self.display_scale = max(0.1, self.display_scale / 1.1)

        self.update()

    def _get_tile_at_screen_pos(self, pos):
        """Convert screen position to tile coordinates"""
        layer = self.get_current_layer()
        if not layer:
            return None

        display_tile_width = int(self.tile_width * self.display_scale)
        display_tile_height = int(self.tile_height * self.display_scale)

        grid_offset_x = int((layer.offset_x * self.tile_width - self.camera_x) * self.display_scale)
        grid_offset_y = int((layer.offset_y * self.tile_height - self.camera_y) * self.display_scale)

        tile_x = (pos.x() - grid_offset_x) // display_tile_width
        tile_y = (pos.y() - grid_offset_y) // display_tile_height

        return (tile_x, tile_y)

    def _paint_at_pos(self, pos):
        """Paint tile at position"""
        layer = self.get_current_layer()
        if not layer:
            return

        tile_pos = self._get_tile_at_screen_pos(pos)
        if not tile_pos:
            return

        tile_x, tile_y = tile_pos

        if self.tool_mode == "paint":
            if self.selected_tileset_index >= 0 and self.selected_tile_index >= 0:
                layer.set_tile(tile_x, tile_y, self.selected_tileset_index, self.selected_tile_index)
                self.update()
                self.tile_painted.emit()
        elif self.tool_mode == "erase":
            layer.clear_tile(tile_x, tile_y)
            self.update()
            self.tile_painted.emit()

    def _complete_rectangle(self):
        """Complete rectangle drawing"""
        layer = self.get_current_layer()
        if not layer or not self.rect_start_pos or not self.rect_current_pos:
            return

        if self.selected_tileset_index < 0 or self.selected_tile_index < 0:
            return

        # Get rectangle bounds
        min_x = min(self.rect_start_pos[0], self.rect_current_pos[0])
        max_x = max(self.rect_start_pos[0], self.rect_current_pos[0])
        min_y = min(self.rect_start_pos[1], self.rect_current_pos[1])
        max_y = max(self.rect_start_pos[1], self.rect_current_pos[1])

        # Fill rectangle
        for y in range(min_y, max_y + 1):
            for x in range(min_x, max_x + 1):
                layer.set_tile(x, y, self.selected_tileset_index, self.selected_tile_index)

        self.update()
        self.tile_painted.emit()

    def _fill_at_pos(self, pos):
        """Fill bucket tool - flood fill"""
        layer = self.get_current_layer()
        if not layer:
            return

        if self.selected_tileset_index < 0 or self.selected_tile_index < 0:
            return

        tile_pos = self._get_tile_at_screen_pos(pos)
        if not tile_pos:
            return

        start_x, start_y = tile_pos

        # Get the tile at start position
        start_tile = layer.get_tile(start_x, start_y)

        # Don't fill if clicking on the same tile type we're trying to fill with
        if start_tile:
            if (start_tile['tileset_index'] == self.selected_tileset_index and
                start_tile['tile_index'] == self.selected_tile_index):
                return

        # Flood fill algorithm
        stack = [(start_x, start_y)]
        visited = set()

        while stack:
            x, y = stack.pop()

            if (x, y) in visited:
                continue

            if x < 0 or x >= layer.width or y < 0 or y >= layer.height:
                continue

            current_tile = layer.get_tile(x, y)

            # Check if this tile matches the start tile
            if start_tile is None:
                if current_tile is not None:
                    continue
            else:
                if current_tile is None:
                    continue
                if (current_tile['tileset_index'] != start_tile['tileset_index'] or
                    current_tile['tile_index'] != start_tile['tile_index']):
                    continue

            visited.add((x, y))
            layer.set_tile(x, y, self.selected_tileset_index, self.selected_tile_index)

            # Add neighbors
            stack.append((x + 1, y))
            stack.append((x - 1, y))
            stack.append((x, y + 1))
            stack.append((x, y - 1))

        self.update()
        self.tile_painted.emit()


class LayerListWidget(QWidget):
    """Widget for managing layers"""

    layer_selected = pyqtSignal(int)
    layer_added = pyqtSignal(str)
    layer_removed = pyqtSignal(int)
    layer_moved = pyqtSignal(int, int)
    layer_visibility_changed = pyqtSignal(int, bool)
    layer_opacity_changed = pyqtSignal(int, float)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.layers = []
        self._setup_ui()

    def _setup_ui(self):
        """Setup UI"""
        layout = QVBoxLayout()
        layout.setContentsMargins(5, 5, 5, 5)

        # Layer list
        self.layer_list = QListWidget()
        self.layer_list.currentRowChanged.connect(self._on_layer_selected)
        layout.addWidget(self.layer_list)

        # Layer controls
        controls_layout = QHBoxLayout()

        add_btn = QPushButton("+ Add")
        add_btn.clicked.connect(self._add_layer)
        controls_layout.addWidget(add_btn)

        remove_btn = QPushButton("- Delete")
        remove_btn.clicked.connect(self._remove_layer)
        controls_layout.addWidget(remove_btn)

        up_btn = QPushButton("↑ Up")
        up_btn.clicked.connect(self._move_layer_up)
        controls_layout.addWidget(up_btn)

        down_btn = QPushButton("↓ Down")
        down_btn.clicked.connect(self._move_layer_down)
        controls_layout.addWidget(down_btn)

        controls_layout.addStretch()

        layout.addLayout(controls_layout)

        # Layer properties
        props_group = QGroupBox("Layer Properties")
        props_layout = QVBoxLayout()

        # Visibility checkbox
        self.visibility_check = QCheckBox("Visible")
        self.visibility_check.setChecked(True)
        self.visibility_check.stateChanged.connect(self._on_visibility_changed)
        props_layout.addWidget(self.visibility_check)

        # Opacity slider
        opacity_layout = QHBoxLayout()
        opacity_layout.addWidget(QLabel("Opacity:"))
        self.opacity_slider = QSlider(Qt.Orientation.Horizontal)
        self.opacity_slider.setMinimum(0)
        self.opacity_slider.setMaximum(100)
        self.opacity_slider.setValue(100)
        self.opacity_slider.valueChanged.connect(self._on_opacity_changed)
        opacity_layout.addWidget(self.opacity_slider)
        self.opacity_label = QLabel("100%")
        opacity_layout.addWidget(self.opacity_label)
        props_layout.addLayout(opacity_layout)

        # Offset controls
        offset_layout = QGridLayout()
        offset_layout.addWidget(QLabel("Offset X:"), 0, 0)
        self.offset_x_spin = QSpinBox()
        self.offset_x_spin.setMinimum(-1000)
        self.offset_x_spin.setMaximum(1000)
        self.offset_x_spin.setValue(0)
        offset_layout.addWidget(self.offset_x_spin, 0, 1)

        offset_layout.addWidget(QLabel("Offset Y:"), 1, 0)
        self.offset_y_spin = QSpinBox()
        self.offset_y_spin.setMinimum(-1000)
        self.offset_y_spin.setMaximum(1000)
        self.offset_y_spin.setValue(0)
        offset_layout.addWidget(self.offset_y_spin, 1, 1)

        props_layout.addLayout(offset_layout)

        props_group.setLayout(props_layout)
        layout.addWidget(props_group)

        self.setLayout(layout)

    def set_layers(self, layers):
        """Set layers"""
        self.layers = layers
        self._refresh_list()

    def _refresh_list(self):
        """Refresh layer list"""
        self.layer_list.clear()
        for i, layer in enumerate(self.layers):
            item = QListWidgetItem(f"{layer.name}")
            if not layer.visible:
                item.setForeground(QColor(150, 150, 150))
            self.layer_list.addItem(item)

    def _add_layer(self):
        """Add a new layer"""
        from PyQt6.QtWidgets import QInputDialog
        name, ok = QInputDialog.getText(self, "New Layer", "Layer name:")
        if ok and name:
            self.layer_added.emit(name)

    def _remove_layer(self):
        """Remove selected layer"""
        current_row = self.layer_list.currentRow()
        if current_row >= 0:
            self.layer_removed.emit(current_row)

    def _move_layer_up(self):
        """Move layer up"""
        current_row = self.layer_list.currentRow()
        if current_row > 0:
            self.layer_moved.emit(current_row, current_row - 1)

    def _move_layer_down(self):
        """Move layer down"""
        current_row = self.layer_list.currentRow()
        if current_row >= 0 and current_row < len(self.layers) - 1:
            self.layer_moved.emit(current_row, current_row + 1)

    def _on_layer_selected(self, row):
        """Handle layer selection"""
        if row >= 0 and row < len(self.layers):
            layer = self.layers[row]
            self.visibility_check.setChecked(layer.visible)
            self.opacity_slider.setValue(int(layer.opacity * 100))
            self.offset_x_spin.setValue(layer.offset_x)
            self.offset_y_spin.setValue(layer.offset_y)
            self.layer_selected.emit(row)

    def _on_visibility_changed(self, state):
        """Handle visibility change"""
        current_row = self.layer_list.currentRow()
        if current_row >= 0:
            visible = state == Qt.CheckState.Checked.value
            self.layer_visibility_changed.emit(current_row, visible)

    def _on_opacity_changed(self, value):
        """Handle opacity change"""
        current_row = self.layer_list.currentRow()
        if current_row >= 0:
            opacity = value / 100.0
            self.opacity_label.setText(f"{value}%")
            self.layer_opacity_changed.emit(current_row, opacity)


class TileMap2DEditor(EditorPanel):
    """TileMap 2D Editor tool for creating tilemaps with multiple layers"""

    def __init__(self, parent=None, project_root=None):
        self.project_root = project_root
        self.current_file = None
        self.tilesets = []
        self.map_width = 32
        self.map_height = 32

        # Get project settings for texture filtering
        self.project = None
        if parent and hasattr(parent, 'project'):
            self.project = parent.project

        super().__init__("TileMap 2D Editor", parent)
        self.setObjectName("TileMap2DEditor")

        # Apply texture filtering from project settings
        if self.project:
            self._apply_texture_filtering()

    def _apply_texture_filtering(self):
        """Apply texture filtering from project settings"""
        if not self.project and self.parent() and hasattr(self.parent(), 'project'):
            self.project = self.parent().project

        if self.project and hasattr(self.project, 'texture_filtering'):
            filter_mode = self.project.texture_filtering
            self.palette_widget.set_texture_filtering(filter_mode)
            self.canvas_widget.set_texture_filtering(filter_mode)

    def _setup_panel(self):
        """Setup tilemap editor panel UI"""
        main_widget = QWidget()
        main_layout = QVBoxLayout()
        main_layout.setContentsMargins(5, 5, 5, 5)

        # Toolbar
        toolbar = QHBoxLayout()

        new_btn = QPushButton("New")
        new_btn.clicked.connect(self._new_tilemap)
        toolbar.addWidget(new_btn)

        load_btn = QPushButton("Load")
        load_btn.clicked.connect(self._load_tilemap)
        toolbar.addWidget(load_btn)

        save_btn = QPushButton("Save")
        save_btn.clicked.connect(self._save_tilemap)
        toolbar.addWidget(save_btn)

        save_as_btn = QPushButton("Save As...")
        save_as_btn.clicked.connect(self._save_as_tilemap)
        toolbar.addWidget(save_as_btn)

        toolbar.addStretch()

        export_menu_btn = QPushButton("Export ▼")
        export_menu = QMenu()
        export_png_action = export_menu.addAction("Export to PNG")
        export_png_action.triggered.connect(self._export_png)
        export_layers_action = export_menu.addAction("Export Layers to PNGs")
        export_layers_action.triggered.connect(self._export_layers_png)
        export_menu_btn.setMenu(export_menu)
        toolbar.addWidget(export_menu_btn)

        main_layout.addLayout(toolbar)

        # Main splitter
        main_splitter = QSplitter(Qt.Orientation.Horizontal)

        # Left side - Tilesets and Palette
        left_widget = QWidget()
        left_layout = QVBoxLayout()
        left_layout.setContentsMargins(0, 0, 0, 0)

        # Tileset management
        tileset_group = QGroupBox("Tilesets")
        tileset_layout = QVBoxLayout()

        tileset_btn_layout = QHBoxLayout()
        load_tileset_btn = QPushButton("Load Tileset")
        load_tileset_btn.clicked.connect(self._load_tileset)
        tileset_btn_layout.addWidget(load_tileset_btn)

        clear_tilesets_btn = QPushButton("Clear All")
        clear_tilesets_btn.clicked.connect(self._clear_tilesets)
        tileset_btn_layout.addWidget(clear_tilesets_btn)

        tileset_layout.addLayout(tileset_btn_layout)

        self.tileset_list = QListWidget()
        self.tileset_list.setMaximumHeight(100)
        tileset_layout.addWidget(self.tileset_list)

        tileset_group.setLayout(tileset_layout)
        left_layout.addWidget(tileset_group)

        # Tile palette
        palette_group = QGroupBox("Tile Palette")
        palette_layout = QVBoxLayout()

        palette_scroll = QScrollArea()
        palette_scroll.setWidgetResizable(True)
        palette_scroll.setHorizontalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAsNeeded)

        self.palette_widget = TilePaletteWidget()
        self.palette_widget.tile_selected.connect(self._on_tile_selected)
        palette_scroll.setWidget(self.palette_widget)

        palette_layout.addWidget(palette_scroll)
        palette_group.setLayout(palette_layout)
        left_layout.addWidget(palette_group, 1)

        left_widget.setLayout(left_layout)
        main_splitter.addWidget(left_widget)

        # Center - Canvas
        center_widget = QWidget()
        center_layout = QVBoxLayout()
        center_layout.setContentsMargins(0, 0, 0, 0)

        # Canvas controls
        canvas_controls = QHBoxLayout()

        # Tool selection
        canvas_controls.addWidget(QLabel("Tool:"))
        self.tool_combo = QComboBox()
        self.tool_combo.addItems(["Paint", "Erase", "Rectangle", "Fill"])
        self.tool_combo.currentTextChanged.connect(self._on_tool_changed)
        canvas_controls.addWidget(self.tool_combo)

        canvas_controls.addStretch()

        # Grid toggle
        self.grid_check = QCheckBox("Show Grid")
        self.grid_check.setChecked(True)
        self.grid_check.stateChanged.connect(self._on_grid_toggled)
        canvas_controls.addWidget(self.grid_check)

        # Zoom controls
        canvas_controls.addWidget(QLabel("Zoom:"))
        zoom_out_btn = QPushButton("-")
        zoom_out_btn.setMaximumWidth(30)
        zoom_out_btn.clicked.connect(lambda: self._adjust_zoom(0.9))
        canvas_controls.addWidget(zoom_out_btn)

        self.zoom_label = QLabel("100%")
        canvas_controls.addWidget(self.zoom_label)

        zoom_in_btn = QPushButton("+")
        zoom_in_btn.setMaximumWidth(30)
        zoom_in_btn.clicked.connect(lambda: self._adjust_zoom(1.1))
        canvas_controls.addWidget(zoom_in_btn)

        center_layout.addLayout(canvas_controls)

        # Canvas
        self.canvas_widget = TileMapCanvas()
        self.canvas_widget.tile_painted.connect(self._on_tile_painted)
        center_layout.addWidget(self.canvas_widget, 1)

        center_widget.setLayout(center_layout)
        main_splitter.addWidget(center_widget)

        # Right side - Layers
        right_widget = QWidget()
        right_layout = QVBoxLayout()
        right_layout.setContentsMargins(0, 0, 0, 0)

        # Map settings
        map_settings_group = QGroupBox("Map Settings")
        map_settings_layout = QFormLayout()

        self.map_width_spin = QSpinBox()
        self.map_width_spin.setMinimum(1)
        self.map_width_spin.setMaximum(1000)
        self.map_width_spin.setValue(32)
        map_settings_layout.addRow("Width:", self.map_width_spin)

        self.map_height_spin = QSpinBox()
        self.map_height_spin.setMinimum(1)
        self.map_height_spin.setMaximum(1000)
        self.map_height_spin.setValue(32)
        map_settings_layout.addRow("Height:", self.map_height_spin)

        self.tile_width_spin = QSpinBox()
        self.tile_width_spin.setMinimum(1)
        self.tile_width_spin.setMaximum(512)
        self.tile_width_spin.setValue(32)
        map_settings_layout.addRow("Tile Width:", self.tile_width_spin)

        self.tile_height_spin = QSpinBox()
        self.tile_height_spin.setMinimum(1)
        self.tile_height_spin.setMaximum(512)
        self.tile_height_spin.setValue(32)
        map_settings_layout.addRow("Tile Height:", self.tile_height_spin)

        map_settings_group.setLayout(map_settings_layout)
        right_layout.addWidget(map_settings_group)

        # Layers
        layers_group = QGroupBox("Layers")
        layers_layout = QVBoxLayout()

        self.layer_widget = LayerListWidget()
        self.layer_widget.layer_selected.connect(self._on_layer_selected)
        self.layer_widget.layer_added.connect(self._on_layer_added)
        self.layer_widget.layer_removed.connect(self._on_layer_removed)
        self.layer_widget.layer_moved.connect(self._on_layer_moved)
        self.layer_widget.layer_visibility_changed.connect(self._on_layer_visibility_changed)
        self.layer_widget.layer_opacity_changed.connect(self._on_layer_opacity_changed)
        layers_layout.addWidget(self.layer_widget)

        layers_group.setLayout(layers_layout)
        right_layout.addWidget(layers_group, 1)

        right_widget.setLayout(right_layout)
        main_splitter.addWidget(right_widget)

        main_splitter.setSizes([250, 500, 250])
        main_layout.addWidget(main_splitter, 1)

        main_widget.setLayout(main_layout)
        self.content_layout.addWidget(main_widget)

        # Create default layer
        self._create_default_layer()

    def _create_default_layer(self):
        """Create a default layer"""
        layer = TileLayer("Layer 0", self.map_width, self.map_height)
        self.canvas_widget.add_layer(layer)
        self.layer_widget.set_layers(self.canvas_widget.layers)
        self.layer_widget.layer_list.setCurrentRow(0)

    def _new_tilemap(self):
        """Create a new tilemap"""
        reply = QMessageBox.question(
            self,
            "New TileMap",
            "Create new tilemap? Unsaved changes will be lost.",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
        )
        if reply == QMessageBox.StandardButton.No:
            return

        self.current_file = None
        self.canvas_widget.layers.clear()
        self.canvas_widget.current_layer_index = 0
        self._create_default_layer()
        self.canvas_widget.update()

    def _load_tilemap(self):
        """Load a .tilemap file"""
        file_path, _ = QFileDialog.getOpenFileName(
            self,
            "Load TileMap",
            str(self._get_tilemaps_directory()) if self.project_root else "",
            "TileMap (*.tilemap)"
        )

        if file_path:
            self._do_load(file_path)

    def _do_load(self, file_path):
        """Perform the load operation"""
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                data = json.load(f)

            self.current_file = file_path
            self.map_width = data.get('width', 32)
            self.map_height = data.get('height', 32)

            self.map_width_spin.setValue(self.map_width)
            self.map_height_spin.setValue(self.map_height)

            tile_width = data.get('tile_width', 32)
            tile_height = data.get('tile_height', 32)
            self.tile_width_spin.setValue(tile_width)
            self.tile_height_spin.setValue(tile_height)
            self.canvas_widget.tile_width = tile_width
            self.canvas_widget.tile_height = tile_height

            # Load tilesets
            self._clear_tilesets()
            tileset_paths = data.get('tilesets', [])
            for tileset_path in tileset_paths:
                if Path(tileset_path).exists():
                    tileset = TilesetData(tileset_path)
                    if tileset.load():
                        self.tilesets.append(tileset)
                        self.tileset_list.addItem(tileset.name)
                        self.palette_widget.add_tileset(tileset)

            self.canvas_widget.tilesets = self.tilesets

            # Load layers
            self.canvas_widget.layers.clear()
            layers_data = data.get('layers', [])
            for layer_data in layers_data:
                layer = TileLayer(
                    layer_data.get('name', 'Layer'),
                    self.map_width,
                    self.map_height
                )
                layer.visible = layer_data.get('visible', True)
                layer.opacity = layer_data.get('opacity', 1.0)
                layer.offset_x = layer_data.get('offset_x', 0)
                layer.offset_y = layer_data.get('offset_y', 0)

                # Load tiles
                tiles_data = layer_data.get('tiles', {})
                for key, tile_data in tiles_data.items():
                    x, y = map(int, key.split(','))
                    layer.tiles[(x, y)] = tile_data

                self.canvas_widget.add_layer(layer)

            if not self.canvas_widget.layers:
                self._create_default_layer()

            self.layer_widget.set_layers(self.canvas_widget.layers)
            self.layer_widget.layer_list.setCurrentRow(0)
            self.canvas_widget.update()

            self._apply_texture_filtering()

            QMessageBox.information(self, "Success", "TileMap loaded successfully")
        except Exception as e:
            QMessageBox.warning(self, "Error", f"Failed to load tilemap:\\n{str(e)}")

    def _save_tilemap(self):
        """Save the current tilemap"""
        if not self.current_file:
            self._save_as_tilemap()
            return

        self._do_save(self.current_file)

    def _save_as_tilemap(self):
        """Save tilemap with a new name"""
        file_path, _ = QFileDialog.getSaveFileName(
            self,
            "Save TileMap",
            str(self._get_tilemaps_directory()) if self.project_root else "",
            "TileMap (*.tilemap)"
        )

        if file_path:
            if not file_path.endswith('.tilemap'):
                file_path += '.tilemap'

            self.current_file = file_path
            self._do_save(file_path)

    def _do_save(self, file_path):
        """Perform the save operation"""
        try:
            # Update map dimensions from spinboxes
            self.map_width = self.map_width_spin.value()
            self.map_height = self.map_height_spin.value()

            # Build tilemap data
            layers_data = []
            for layer in self.canvas_widget.layers:
                # Convert tiles dict to serializable format
                tiles_data = {}
                for (x, y), tile_data in layer.tiles.items():
                    tiles_data[f"{x},{y}"] = tile_data

                layer_json = {
                    'name': layer.name,
                    'visible': layer.visible,
                    'opacity': layer.opacity,
                    'offset_x': layer.offset_x,
                    'offset_y': layer.offset_y,
                    'tiles': tiles_data
                }
                layers_data.append(layer_json)

            # Get tileset paths
            tileset_paths = [ts.file_path for ts in self.tilesets]

            data = {
                'width': self.map_width,
                'height': self.map_height,
                'tile_width': self.tile_width_spin.value(),
                'tile_height': self.tile_height_spin.value(),
                'tilesets': tileset_paths,
                'layers': layers_data
            }

            with open(file_path, 'w', encoding='utf-8') as f:
                json.dump(data, f, indent=2)

            QMessageBox.information(self, "Success", "TileMap saved successfully")
        except Exception as e:
            QMessageBox.warning(self, "Error", f"Failed to save tilemap:\\n{str(e)}")

    def _load_tileset(self):
        """Load a tileset"""
        file_path, _ = QFileDialog.getOpenFileName(
            self,
            "Load Tileset",
            str(self._get_tilesets_directory()) if self.project_root else "",
            "Tileset 2D (*.tileset2D)"
        )

        if file_path:
            tileset = TilesetData(file_path)
            if tileset.load():
                self.tilesets.append(tileset)
                self.tileset_list.addItem(tileset.name)
                self.palette_widget.add_tileset(tileset)
                self.canvas_widget.tilesets = self.tilesets
                self._apply_texture_filtering()
                QMessageBox.information(self, "Success", f"Tileset '{tileset.name}' loaded")
            else:
                QMessageBox.warning(self, "Error", "Failed to load tileset")

    def _clear_tilesets(self):
        """Clear all tilesets"""
        self.tilesets.clear()
        self.tileset_list.clear()
        self.palette_widget.clear_tilesets()
        self.canvas_widget.tilesets = []

    def _on_tile_selected(self, tileset_index, tile_index):
        """Handle tile selection from palette"""
        self.canvas_widget.selected_tileset_index = tileset_index
        self.canvas_widget.selected_tile_index = tile_index

    def _on_tool_changed(self, tool_name):
        """Handle tool change"""
        self.canvas_widget.tool_mode = tool_name.lower()

    def _on_grid_toggled(self, state):
        """Handle grid toggle"""
        self.canvas_widget.grid_visible = state == Qt.CheckState.Checked.value
        self.canvas_widget.update()

    def _adjust_zoom(self, factor):
        """Adjust zoom level"""
        self.canvas_widget.display_scale *= factor
        self.canvas_widget.display_scale = max(0.1, min(8.0, self.canvas_widget.display_scale))
        self.zoom_label.setText(f"{int(self.canvas_widget.display_scale * 100)}%")
        self.canvas_widget.update()

    def _on_tile_painted(self):
        """Handle tile painted"""
        pass  # Can add undo/redo here later

    def _on_layer_selected(self, index):
        """Handle layer selection"""
        self.canvas_widget.current_layer_index = index

        # Update offset spinboxes
        if 0 <= index < len(self.canvas_widget.layers):
            layer = self.canvas_widget.layers[index]
            self.layer_widget.offset_x_spin.blockSignals(True)
            self.layer_widget.offset_y_spin.blockSignals(True)
            self.layer_widget.offset_x_spin.setValue(layer.offset_x)
            self.layer_widget.offset_y_spin.setValue(layer.offset_y)
            self.layer_widget.offset_x_spin.blockSignals(False)
            self.layer_widget.offset_y_spin.blockSignals(False)

            # Connect offset changes
            self.layer_widget.offset_x_spin.valueChanged.connect(
                lambda v: self._on_layer_offset_changed(index, 'x', v)
            )
            self.layer_widget.offset_y_spin.valueChanged.connect(
                lambda v: self._on_layer_offset_changed(index, 'y', v)
            )

        self.canvas_widget.update()

    def _on_layer_offset_changed(self, layer_index, axis, value):
        """Handle layer offset change"""
        if 0 <= layer_index < len(self.canvas_widget.layers):
            layer = self.canvas_widget.layers[layer_index]
            if axis == 'x':
                layer.offset_x = value
            else:
                layer.offset_y = value
            self.canvas_widget.update()

    def _on_layer_added(self, name):
        """Handle layer addition"""
        layer = TileLayer(name, self.map_width, self.map_height)
        self.canvas_widget.add_layer(layer)
        self.layer_widget.set_layers(self.canvas_widget.layers)
        self.layer_widget.layer_list.setCurrentRow(len(self.canvas_widget.layers) - 1)

    def _on_layer_removed(self, index):
        """Handle layer removal"""
        if 0 <= index < len(self.canvas_widget.layers):
            del self.canvas_widget.layers[index]

            # If no layers left, reset current layer index and create a default layer
            if not self.canvas_widget.layers:
                self.canvas_widget.current_layer_index = 0
                self._create_default_layer()
            else:
                # Adjust current layer index if necessary
                if self.canvas_widget.current_layer_index >= len(self.canvas_widget.layers):
                    self.canvas_widget.current_layer_index = len(self.canvas_widget.layers) - 1

            self.layer_widget.set_layers(self.canvas_widget.layers)

            # Select a valid layer
            if self.canvas_widget.layers:
                self.layer_widget.layer_list.setCurrentRow(self.canvas_widget.current_layer_index)

            self.canvas_widget.update()

    def _on_layer_moved(self, from_index, to_index):
        """Handle layer reordering"""
        if 0 <= from_index < len(self.canvas_widget.layers) and 0 <= to_index < len(self.canvas_widget.layers):
            layer = self.canvas_widget.layers.pop(from_index)
            self.canvas_widget.layers.insert(to_index, layer)
            self.layer_widget.set_layers(self.canvas_widget.layers)
            self.layer_widget.layer_list.setCurrentRow(to_index)
            self.canvas_widget.update()

    def _on_layer_visibility_changed(self, index, visible):
        """Handle layer visibility change"""
        if 0 <= index < len(self.canvas_widget.layers):
            self.canvas_widget.layers[index].visible = visible
            self.layer_widget._refresh_list()
            self.canvas_widget.update()

    def _on_layer_opacity_changed(self, index, opacity):
        """Handle layer opacity change"""
        if 0 <= index < len(self.canvas_widget.layers):
            self.canvas_widget.layers[index].opacity = opacity
            self.canvas_widget.update()

    def _export_png(self):
        """Export entire tilemap to PNG"""
        file_path, _ = QFileDialog.getSaveFileName(
            self,
            "Export to PNG",
            str(self.project_root) if self.project_root else "",
            "PNG Image (*.png)"
        )

        if file_path:
            if not file_path.endswith('.png'):
                file_path += '.png'

            self._do_export_png(file_path, None)

    def _export_layers_png(self):
        """Export each layer as separate PNG"""
        dir_path = QFileDialog.getExistingDirectory(
            self,
            "Select Directory for Layer Export",
            str(self.project_root) if self.project_root else ""
        )

        if dir_path:
            for i, layer in enumerate(self.canvas_widget.layers):
                file_path = Path(dir_path) / f"{layer.name}.png"
                self._do_export_png(str(file_path), i)

            QMessageBox.information(self, "Success", f"Exported {len(self.canvas_widget.layers)} layers")

    def _do_export_png(self, file_path, layer_index=None):
        """Perform PNG export"""
        try:
            # Calculate image size
            width = self.map_width * self.canvas_widget.tile_width
            height = self.map_height * self.canvas_widget.tile_height

            # Create image
            image = QImage(width, height, QImage.Format.Format_ARGB32)
            image.fill(Qt.GlobalColor.transparent)

            painter = QPainter(image)

            # Determine which layers to render
            layers_to_render = []
            if layer_index is not None:
                if 0 <= layer_index < len(self.canvas_widget.layers):
                    layers_to_render = [self.canvas_widget.layers[layer_index]]
            else:
                layers_to_render = self.canvas_widget.layers

            # Render layers
            for layer in layers_to_render:
                if not layer.visible:
                    continue

                painter.setOpacity(layer.opacity)

                for (tile_x, tile_y), tile_data in layer.tiles.items():
                    tileset_idx = tile_data['tileset_index']
                    tile_idx = tile_data['tile_index']

                    if tileset_idx < 0 or tileset_idx >= len(self.tilesets):
                        continue

                    tileset = self.tilesets[tileset_idx]
                    tile_pixmap = tileset.get_tile_pixmap(tile_idx)

                    if tile_pixmap.isNull():
                        continue

                    # Calculate position with layer offset
                    actual_x = tile_x + layer.offset_x
                    actual_y = tile_y + layer.offset_y

                    x = actual_x * self.canvas_widget.tile_width
                    y = actual_y * self.canvas_widget.tile_height

                    painter.drawPixmap(x, y, tile_pixmap)

            painter.end()

            # Save image
            image.save(file_path)

            if layer_index is None:
                QMessageBox.information(self, "Success", "TileMap exported to PNG")
        except Exception as e:
            QMessageBox.warning(self, "Error", f"Failed to export PNG:\\n{str(e)}")

    def _get_tilemaps_directory(self):
        """Get the tilemaps directory path"""
        if self.project_root:
            tilemap_dir = Path(self.project_root) / "assets" / "tilemaps"
            tilemap_dir.mkdir(parents=True, exist_ok=True)
            return tilemap_dir
        return Path.home()

    def _get_tilesets_directory(self):
        """Get the tilesets directory path"""
        if self.project_root:
            tileset_dir = Path(self.project_root) / "assets" / "tilesets"
            tileset_dir.mkdir(parents=True, exist_ok=True)
            return tileset_dir
        return Path.home()
