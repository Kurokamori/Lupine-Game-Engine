"""
Tileset 2D Builder Tool
A comprehensive tool for creating and managing 2D tilesets with collision shapes
"""

from PyQt6.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QLabel,
                             QPushButton, QListWidget, QListWidgetItem,
                             QFileDialog, QMessageBox, QDialog, QFormLayout,
                             QLineEdit, QComboBox, QDialogButtonBox, QSpinBox,
                             QDoubleSpinBox, QSplitter, QGroupBox, QGridLayout,
                             QScrollArea, QRadioButton, QButtonGroup, QFrame,
                             QCheckBox, QTextEdit, QTabWidget)
from PyQt6.QtCore import Qt, QTimer, pyqtSignal, QSize, QRect, QPoint, QPointF
from PyQt6.QtGui import QPixmap, QImage, QPainter, QColor, QPen, QPolygonF, QBrush
from pathlib import Path
import json
import sys
import uuid

# Import base panel
editor_path = Path(__file__).parent.parent.parent
if str(editor_path) not in sys.path:
    sys.path.insert(0, str(editor_path))

from panels.base_panel import EditorPanel


class TileData:
    """Represents a single tile with metadata and collision data"""
    def __init__(self, index: int, rect: QRect):
        self.index = index
        self.rect = rect  # QRect defining the tile area in the tileset
        self.tags = []  # List of string tags
        self.metadata = {}  # Dictionary of custom metadata
        self.collision_shape_type = "None"  # None, Rectangle, Circle, Polygon
        self.collision_vertices = []  # List of QPointF for polygon collision
        self.collision_radius = 0.5  # For circle collision (normalized 0-1)
        self.collision_rect = QRect(0, 0, 1, 1)  # For rectangle collision (normalized)


class TilesetWidget(QWidget):
    """Widget for displaying and selecting tiles from a tileset"""

    tile_clicked = pyqtSignal(int)  # Emits tile index when clicked
    tile_selected = pyqtSignal(int)  # Emits tile index when selected

    def __init__(self, parent=None):
        super().__init__(parent)
        self.tiles = []  # List of TileData objects
        self.tileset_image = None  # QPixmap of the tileset
        self.selected_tile_index = -1
        self.hover_index = -1
        self.transformation_mode = Qt.TransformationMode.SmoothTransformation

        # Grid layout settings
        self.tile_width = 32
        self.tile_height = 32
        self.display_scale = 2.0  # Scale factor for display
        self.grid_cols = 8

        self.setMinimumSize(400, 400)
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

    def load_tileset(self, image_path: str, tile_width: int, tile_height: int):
        """Load a tileset image and slice it into tiles"""
        pixmap = QPixmap(str(image_path))
        if pixmap.isNull():
            return False

        self.tileset_image = pixmap
        self.tile_width = tile_width
        self.tile_height = tile_height
        self.tiles.clear()
        self.selected_tile_index = -1

        cols = pixmap.width() // tile_width
        rows = pixmap.height() // tile_height
        self.grid_cols = cols

        idx = 0
        for row in range(rows):
            for col in range(cols):
                x = col * tile_width
                y = row * tile_height
                rect = QRect(x, y, tile_width, tile_height)
                tile = TileData(idx, rect)
                self.tiles.append(tile)
                idx += 1

        self.update()
        return True

    def get_tile_count(self):
        """Get the number of tiles"""
        return len(self.tiles)

    def get_tile_pixmap(self, index: int) -> QPixmap:
        """Get a pixmap for a specific tile"""
        if index < 0 or index >= len(self.tiles) or not self.tileset_image:
            return QPixmap()

        tile = self.tiles[index]
        return self.tileset_image.copy(tile.rect)

    def get_selected_tile(self):
        """Get the currently selected tile data"""
        if self.selected_tile_index >= 0 and self.selected_tile_index < len(self.tiles):
            return self.tiles[self.selected_tile_index]
        return None

    def paintEvent(self, event):
        """Draw the tileset"""
        painter = QPainter(self)
        painter.fillRect(self.rect(), QColor(40, 40, 40))

        if not self.tiles or not self.tileset_image:
            painter.setPen(QColor(200, 200, 200))
            painter.drawText(self.rect(), Qt.AlignmentFlag.AlignCenter,
                           "Load a tileset image")
            return

        # Calculate display size
        display_tile_width = int(self.tile_width * self.display_scale)
        display_tile_height = int(self.tile_height * self.display_scale)
        padding = 4

        for idx, tile in enumerate(self.tiles):
            row = idx // self.grid_cols
            col = idx % self.grid_cols

            x = col * (display_tile_width + padding) + padding
            y = row * (display_tile_height + padding) + padding

            # Draw tile
            pixmap = self.get_tile_pixmap(idx)
            if not pixmap.isNull():
                scaled = pixmap.scaled(display_tile_width, display_tile_height,
                                      Qt.AspectRatioMode.IgnoreAspectRatio,
                                      self.transformation_mode)
                painter.drawPixmap(x, y, scaled)

            # Draw selection/hover border
            if idx == self.selected_tile_index:
                painter.setPen(QPen(QColor(0, 120, 215), 3))
                painter.drawRect(x - 2, y - 2, display_tile_width + 4, display_tile_height + 4)
            elif idx == self.hover_index:
                painter.setPen(QPen(QColor(100, 100, 100), 2))
                painter.drawRect(x - 1, y - 1, display_tile_width + 2, display_tile_height + 2)

            # Draw collision indicator
            if tile.collision_shape_type != "None":
                painter.setPen(QPen(QColor(255, 0, 0), 2))
                painter.drawRect(x + 2, y + 2, 8, 8)

            # Draw tag indicator
            if tile.tags:
                painter.setPen(QPen(QColor(0, 255, 0), 2))
                painter.drawRect(x + display_tile_width - 10, y + 2, 8, 8)

            # Draw index
            painter.setPen(QColor(255, 255, 255))
            painter.drawText(x + 2, y + 14, str(idx))

        # Update widget size
        total_rows = (len(self.tiles) + self.grid_cols - 1) // self.grid_cols
        min_height = total_rows * (display_tile_height + padding) + padding
        self.setMinimumHeight(min_height)

    def mousePressEvent(self, event):
        """Handle mouse click to select tile"""
        if event.button() == Qt.MouseButton.LeftButton:
            index = self._get_tile_at_pos(event.pos())
            if index >= 0:
                self.selected_tile_index = index
                self.tile_clicked.emit(index)
                self.tile_selected.emit(index)
                self.update()

    def mouseMoveEvent(self, event):
        """Handle mouse move for hover effect"""
        old_hover = self.hover_index
        self.hover_index = self._get_tile_at_pos(event.pos())
        if old_hover != self.hover_index:
            self.update()

    def _get_tile_at_pos(self, pos):
        """Get tile index at mouse position"""
        display_tile_width = int(self.tile_width * self.display_scale)
        display_tile_height = int(self.tile_height * self.display_scale)
        padding = 4

        for idx in range(len(self.tiles)):
            row = idx // self.grid_cols
            col = idx % self.grid_cols

            x = col * (display_tile_width + padding) + padding
            y = row * (display_tile_height + padding) + padding

            if (x <= pos.x() <= x + display_tile_width and
                y <= pos.y() <= y + display_tile_height):
                return idx

        return -1


class CollisionShapeWidget(QWidget):
    """Widget for editing collision shapes for a tile"""

    collision_changed = pyqtSignal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self.tile_data = None
        self.tile_pixmap = None
        self.is_editing = False
        self.transformation_mode = Qt.TransformationMode.SmoothTransformation

        self.setMinimumSize(250, 250)
        self.setMouseTracking(True)

        # Polygon editing state
        self.selected_vertex_index = -1
        self.hover_vertex_index = -1
        self.dragging_vertex = False

    def set_tile(self, tile_data: TileData, tile_pixmap: QPixmap):
        """Set the tile to edit collision for"""
        self.tile_data = tile_data
        self.tile_pixmap = tile_pixmap
        self.selected_vertex_index = -1
        self.hover_vertex_index = -1
        self.update()

    def set_texture_filtering(self, filter_mode: str):
        """Set texture filtering mode"""
        filter_map = {
            "nearest": Qt.TransformationMode.FastTransformation,
            "bilinear": Qt.TransformationMode.SmoothTransformation,
            "cubic": Qt.TransformationMode.SmoothTransformation
        }
        self.transformation_mode = filter_map.get(filter_mode.lower(), Qt.TransformationMode.SmoothTransformation)
        self.update()

    def paintEvent(self, event):
        """Draw the tile and collision shape"""
        painter = QPainter(self)
        painter.fillRect(self.rect(), QColor(50, 50, 50))

        if not self.tile_data or not self.tile_pixmap:
            painter.setPen(QColor(200, 200, 200))
            painter.drawText(self.rect(), Qt.AlignmentFlag.AlignCenter,
                           "Select a tile to edit collision")
            return

        # Scale tile to fit widget
        margin = 20
        available_width = self.width() - 2 * margin
        available_height = self.height() - 2 * margin

        scale = min(available_width / self.tile_pixmap.width(),
                   available_height / self.tile_pixmap.height())

        display_width = int(self.tile_pixmap.width() * scale)
        display_height = int(self.tile_pixmap.height() * scale)

        # Center the tile
        tile_x = (self.width() - display_width) // 2
        tile_y = (self.height() - display_height) // 2

        # Draw tile
        scaled_pixmap = self.tile_pixmap.scaled(display_width, display_height,
                                                Qt.AspectRatioMode.IgnoreAspectRatio,
                                                self.transformation_mode)
        painter.drawPixmap(tile_x, tile_y, scaled_pixmap)

        # Draw collision shape
        painter.setPen(QPen(QColor(255, 0, 0, 200), 2))
        painter.setBrush(QBrush(QColor(255, 0, 0, 50)))

        if self.tile_data.collision_shape_type == "Rectangle":
            # Draw rectangle collision
            rect = self.tile_data.collision_rect
            x = tile_x + rect.x() * display_width
            y = tile_y + rect.y() * display_height
            w = rect.width() * display_width
            h = rect.height() * display_height
            painter.drawRect(int(x), int(y), int(w), int(h))

        elif self.tile_data.collision_shape_type == "Circle":
            # Draw circle collision
            radius = self.tile_data.collision_radius * min(display_width, display_height)
            center_x = tile_x + display_width / 2
            center_y = tile_y + display_height / 2
            painter.drawEllipse(QPointF(center_x, center_y), radius, radius)

        elif self.tile_data.collision_shape_type == "Polygon" and self.tile_data.collision_vertices:
            # Draw polygon collision
            polygon = QPolygonF()
            for vertex in self.tile_data.collision_vertices:
                x = tile_x + vertex.x() * display_width
                y = tile_y + vertex.y() * display_height
                polygon.append(QPointF(x, y))
            painter.drawPolygon(polygon)

            # Draw vertices
            for idx, vertex in enumerate(self.tile_data.collision_vertices):
                x = tile_x + vertex.x() * display_width
                y = tile_y + vertex.y() * display_height

                if idx == self.selected_vertex_index:
                    painter.setBrush(QBrush(QColor(0, 255, 0)))
                    painter.drawEllipse(QPointF(x, y), 6, 6)
                elif idx == self.hover_vertex_index:
                    painter.setBrush(QBrush(QColor(255, 255, 0)))
                    painter.drawEllipse(QPointF(x, y), 5, 5)
                else:
                    painter.setBrush(QBrush(QColor(255, 0, 0)))
                    painter.drawEllipse(QPointF(x, y), 4, 4)

    def mousePressEvent(self, event):
        """Handle mouse click for polygon editing"""
        if not self.tile_data or not self.tile_pixmap:
            return

        if self.tile_data.collision_shape_type != "Polygon":
            return

        margin = 20
        available_width = self.width() - 2 * margin
        available_height = self.height() - 2 * margin
        scale = min(available_width / self.tile_pixmap.width(),
                   available_height / self.tile_pixmap.height())
        display_width = int(self.tile_pixmap.width() * scale)
        display_height = int(self.tile_pixmap.height() * scale)
        tile_x = (self.width() - display_width) // 2
        tile_y = (self.height() - display_height) // 2

        click_x = event.pos().x()
        click_y = event.pos().y()

        if event.button() == Qt.MouseButton.LeftButton:
            # Check if clicking on existing vertex
            vertex_idx = self._get_vertex_at_pos(event.pos())
            if vertex_idx >= 0:
                self.selected_vertex_index = vertex_idx
                self.dragging_vertex = True
                self.update()
                return

            # Add new vertex
            if tile_x <= click_x <= tile_x + display_width and tile_y <= click_y <= tile_y + display_height:
                # Normalize coordinates (0-1)
                norm_x = (click_x - tile_x) / display_width
                norm_y = (click_y - tile_y) / display_height
                self.tile_data.collision_vertices.append(QPointF(norm_x, norm_y))
                self.collision_changed.emit()
                self.update()

        elif event.button() == Qt.MouseButton.RightButton:
            # Remove vertex
            vertex_idx = self._get_vertex_at_pos(event.pos())
            if vertex_idx >= 0:
                del self.tile_data.collision_vertices[vertex_idx]
                self.selected_vertex_index = -1
                self.collision_changed.emit()
                self.update()

    def mouseMoveEvent(self, event):
        """Handle mouse move for dragging vertices"""
        if not self.tile_data or not self.tile_pixmap:
            return

        if self.tile_data.collision_shape_type != "Polygon":
            return

        margin = 20
        available_width = self.width() - 2 * margin
        available_height = self.height() - 2 * margin
        scale = min(available_width / self.tile_pixmap.width(),
                   available_height / self.tile_pixmap.height())
        display_width = int(self.tile_pixmap.width() * scale)
        display_height = int(self.tile_pixmap.height() * scale)
        tile_x = (self.width() - display_width) // 2
        tile_y = (self.height() - display_height) // 2

        if self.dragging_vertex and self.selected_vertex_index >= 0:
            # Update vertex position
            click_x = event.pos().x()
            click_y = event.pos().y()
            norm_x = max(0, min(1, (click_x - tile_x) / display_width))
            norm_y = max(0, min(1, (click_y - tile_y) / display_height))
            self.tile_data.collision_vertices[self.selected_vertex_index] = QPointF(norm_x, norm_y)
            self.collision_changed.emit()
            self.update()
        else:
            # Update hover state
            old_hover = self.hover_vertex_index
            self.hover_vertex_index = self._get_vertex_at_pos(event.pos())
            if old_hover != self.hover_vertex_index:
                self.update()

    def mouseReleaseEvent(self, event):
        """Handle mouse release"""
        if event.button() == Qt.MouseButton.LeftButton:
            self.dragging_vertex = False

    def _get_vertex_at_pos(self, pos):
        """Get vertex index at mouse position"""
        if not self.tile_data or not self.tile_pixmap:
            return -1

        margin = 20
        available_width = self.width() - 2 * margin
        available_height = self.height() - 2 * margin
        scale = min(available_width / self.tile_pixmap.width(),
                   available_height / self.tile_pixmap.height())
        display_width = int(self.tile_pixmap.width() * scale)
        display_height = int(self.tile_pixmap.height() * scale)
        tile_x = (self.width() - display_width) // 2
        tile_y = (self.height() - display_height) // 2

        threshold = 8  # pixels

        for idx, vertex in enumerate(self.tile_data.collision_vertices):
            x = tile_x + vertex.x() * display_width
            y = tile_y + vertex.y() * display_height
            dist = ((pos.x() - x) ** 2 + (pos.y() - y) ** 2) ** 0.5
            if dist <= threshold:
                return idx

        return -1


class TilePropertiesWidget(QWidget):
    """Widget for editing tile properties (tags, metadata)"""

    properties_changed = pyqtSignal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self.tile_data = None
        self._setup_ui()

    def _setup_ui(self):
        """Setup the UI"""
        layout = QVBoxLayout()
        layout.setContentsMargins(5, 5, 5, 5)

        # Create tab widget for Tags and Metadata
        self.tab_widget = QTabWidget()

        # Tags tab
        tags_widget = QWidget()
        tags_layout = QVBoxLayout()
        tags_layout.setContentsMargins(5, 5, 5, 5)

        tags_btn_layout = QHBoxLayout()
        add_tag_btn = QPushButton("Add Tag")
        add_tag_btn.clicked.connect(self._add_tag)
        tags_btn_layout.addWidget(add_tag_btn)

        remove_tag_btn = QPushButton("Remove Tag")
        remove_tag_btn.clicked.connect(self._remove_tag)
        tags_btn_layout.addWidget(remove_tag_btn)

        tags_layout.addLayout(tags_btn_layout)

        self.tags_list = QListWidget()
        tags_layout.addWidget(self.tags_list)

        tags_widget.setLayout(tags_layout)
        self.tab_widget.addTab(tags_widget, "Tags")

        # Metadata tab
        metadata_widget = QWidget()
        metadata_layout = QVBoxLayout()
        metadata_layout.setContentsMargins(5, 5, 5, 5)

        metadata_form = QHBoxLayout()
        self.metadata_key_edit = QLineEdit()
        self.metadata_key_edit.setPlaceholderText("Key")
        metadata_form.addWidget(self.metadata_key_edit)

        self.metadata_value_edit = QLineEdit()
        self.metadata_value_edit.setPlaceholderText("Value")
        metadata_form.addWidget(self.metadata_value_edit)

        add_metadata_btn = QPushButton("+")
        add_metadata_btn.setMaximumWidth(30)
        add_metadata_btn.clicked.connect(self._add_metadata)
        metadata_form.addWidget(add_metadata_btn)

        metadata_layout.addLayout(metadata_form)

        self.metadata_list = QListWidget()
        metadata_layout.addWidget(self.metadata_list)

        remove_metadata_btn = QPushButton("Remove Selected")
        remove_metadata_btn.clicked.connect(self._remove_metadata)
        metadata_layout.addWidget(remove_metadata_btn)

        metadata_widget.setLayout(metadata_layout)
        self.tab_widget.addTab(metadata_widget, "Metadata")

        layout.addWidget(self.tab_widget)

        self.setLayout(layout)

    def set_tile(self, tile_data: TileData):
        """Set the tile to edit properties for"""
        self.tile_data = tile_data
        self._refresh_ui()

    def _refresh_ui(self):
        """Refresh UI with current tile data"""
        if not self.tile_data:
            self.tags_list.clear()
            self.metadata_list.clear()
            return

        # Refresh tags
        self.tags_list.clear()
        for tag in self.tile_data.tags:
            self.tags_list.addItem(tag)

        # Refresh metadata
        self.metadata_list.clear()
        for key, value in self.tile_data.metadata.items():
            self.metadata_list.addItem(f"{key}: {value}")

    def _add_tag(self):
        """Add a tag to the tile"""
        if not self.tile_data:
            return

        from PyQt6.QtWidgets import QInputDialog
        tag, ok = QInputDialog.getText(self, "Add Tag", "Tag name:")
        if ok and tag:
            if tag not in self.tile_data.tags:
                self.tile_data.tags.append(tag)
                self._refresh_ui()
                self.properties_changed.emit()

    def _remove_tag(self):
        """Remove selected tag"""
        if not self.tile_data:
            return

        current_row = self.tags_list.currentRow()
        if current_row >= 0:
            del self.tile_data.tags[current_row]
            self._refresh_ui()
            self.properties_changed.emit()

    def _add_metadata(self):
        """Add metadata to the tile"""
        if not self.tile_data:
            return

        key = self.metadata_key_edit.text().strip()
        value = self.metadata_value_edit.text().strip()

        if key and value:
            self.tile_data.metadata[key] = value
            self.metadata_key_edit.clear()
            self.metadata_value_edit.clear()
            self._refresh_ui()
            self.properties_changed.emit()

    def _remove_metadata(self):
        """Remove selected metadata"""
        if not self.tile_data:
            return

        current_row = self.metadata_list.currentRow()
        if current_row >= 0:
            key = list(self.tile_data.metadata.keys())[current_row]
            del self.tile_data.metadata[key]
            self._refresh_ui()
            self.properties_changed.emit()


class Tileset2DBuilder(EditorPanel):
    """Tileset 2D Builder tool for creating tilesets with collision and metadata"""

    def __init__(self, parent=None, project_root=None):
        self.project_root = project_root
        self.current_file = None
        self.tileset_path = None
        self.asset_uuid = None

        # Get project settings for texture filtering
        self.project = None
        if parent and hasattr(parent, 'project'):
            self.project = parent.project

        super().__init__("Tileset 2D Builder", parent)
        self.setObjectName("Tileset2DBuilder")

        # Apply texture filtering from project settings
        if self.project:
            self._apply_texture_filtering()

    def _apply_texture_filtering(self):
        """Apply texture filtering from project settings"""
        if not self.project and self.parent() and hasattr(self.parent(), 'project'):
            self.project = self.parent().project

        if self.project and hasattr(self.project, 'texture_filtering'):
            filter_mode = self.project.texture_filtering
            self.tileset_widget.set_texture_filtering(filter_mode)
            self.collision_widget.set_texture_filtering(filter_mode)

    def _setup_panel(self):
        """Setup tileset builder panel UI"""
        main_widget = QWidget()
        main_layout = QVBoxLayout()
        main_layout.setContentsMargins(5, 5, 5, 5)

        # Toolbar
        toolbar = QHBoxLayout()

        new_btn = QPushButton("New")
        new_btn.clicked.connect(self._new_tileset)
        toolbar.addWidget(new_btn)

        load_btn = QPushButton("Load")
        load_btn.clicked.connect(self._load_tileset)
        toolbar.addWidget(load_btn)

        save_btn = QPushButton("Save")
        save_btn.clicked.connect(self._save_tileset)
        toolbar.addWidget(save_btn)

        save_as_btn = QPushButton("Save As...")
        save_as_btn.clicked.connect(self._save_as_tileset)
        toolbar.addWidget(save_as_btn)

        toolbar.addStretch()

        export_json_btn = QPushButton("Export JSON")
        export_json_btn.clicked.connect(self._export_json)
        toolbar.addWidget(export_json_btn)

        import_json_btn = QPushButton("Import JSON")
        import_json_btn.clicked.connect(self._import_json)
        toolbar.addWidget(import_json_btn)

        main_layout.addLayout(toolbar)

        # Main splitter (horizontal)
        main_splitter = QSplitter(Qt.Orientation.Horizontal)

        # Left side - Tileset display
        left_widget = QWidget()
        left_layout = QVBoxLayout()
        left_layout.setContentsMargins(0, 0, 0, 0)

        # Tileset display group
        tileset_display_group = QGroupBox("Tileset Display")
        tileset_display_layout = QVBoxLayout()

        # Tileset widget (scrollable)
        tileset_scroll = QScrollArea()
        tileset_scroll.setWidgetResizable(True)
        tileset_scroll.setHorizontalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAsNeeded)

        self.tileset_widget = TilesetWidget()
        self.tileset_widget.tile_selected.connect(self._on_tile_selected)
        tileset_scroll.setWidget(self.tileset_widget)

        tileset_display_layout.addWidget(tileset_scroll)
        tileset_display_group.setLayout(tileset_display_layout)

        left_layout.addWidget(tileset_display_group, 1)

        # Tileset loading controls
        tileset_group = QGroupBox("Tileset Controls")
        tileset_layout = QVBoxLayout()

        load_tileset_btn = QPushButton("Load Tileset Image")
        load_tileset_btn.clicked.connect(self._load_tileset_image)
        tileset_layout.addWidget(load_tileset_btn)

        # Tile size and scale controls - all in one row
        size_scale_layout = QHBoxLayout()

        size_scale_layout.addWidget(QLabel("Width:"))
        self.tile_width_spin = QSpinBox()
        self.tile_width_spin.setMinimum(1)
        self.tile_width_spin.setMaximum(512)
        self.tile_width_spin.setValue(32)
        self.tile_width_spin.valueChanged.connect(self._on_tile_size_changed)
        size_scale_layout.addWidget(self.tile_width_spin)

        size_scale_layout.addWidget(QLabel("Height:"))
        self.tile_height_spin = QSpinBox()
        self.tile_height_spin.setMinimum(1)
        self.tile_height_spin.setMaximum(512)
        self.tile_height_spin.setValue(32)
        self.tile_height_spin.valueChanged.connect(self._on_tile_size_changed)
        size_scale_layout.addWidget(self.tile_height_spin)

        size_scale_layout.addWidget(QLabel("Scale:"))
        self.scale_spin = QDoubleSpinBox()
        self.scale_spin.setMinimum(0.5)
        self.scale_spin.setMaximum(8.0)
        self.scale_spin.setValue(2.0)
        self.scale_spin.setSingleStep(0.5)
        self.scale_spin.valueChanged.connect(self._on_scale_changed)
        size_scale_layout.addWidget(self.scale_spin)

        tileset_layout.addLayout(size_scale_layout)

        # Tile info label
        self.tile_info_label = QLabel("No tileset loaded")
        self.tile_info_label.setStyleSheet("color: gray;")
        tileset_layout.addWidget(self.tile_info_label)

        tileset_group.setLayout(tileset_layout)
        left_layout.addWidget(tileset_group)

        left_widget.setLayout(left_layout)
        main_splitter.addWidget(left_widget)

        # Right side - Tile properties and collision editor
        right_widget = QWidget()
        right_layout = QVBoxLayout()
        right_layout.setContentsMargins(0, 0, 0, 0)

        # Selected tile info
        self.selected_tile_label = QLabel("No tile selected")
        self.selected_tile_label.setStyleSheet("font-weight: bold; padding: 5px;")
        right_layout.addWidget(self.selected_tile_label)

        # Collision editor
        collision_group = QGroupBox("Collision Editor")
        collision_layout = QVBoxLayout()

        self.collision_widget = CollisionShapeWidget()
        self.collision_widget.collision_changed.connect(self._on_collision_changed)
        collision_layout.addWidget(self.collision_widget, 1)

        # Collision shape controls (below editor)
        collision_controls_layout = QVBoxLayout()

        collision_type_layout = QHBoxLayout()
        collision_type_layout.addWidget(QLabel("Shape Type:"))

        self.collision_type_combo = QComboBox()
        self.collision_type_combo.addItems(["None", "Rectangle", "Circle", "Polygon"])
        self.collision_type_combo.currentTextChanged.connect(self._on_collision_type_changed)
        collision_type_layout.addWidget(self.collision_type_combo)

        collision_controls_layout.addLayout(collision_type_layout)

        self.collision_help = QLabel("Select collision type above")
        self.collision_help.setWordWrap(True)
        self.collision_help.setStyleSheet("color: gray; font-size: 10px;")
        collision_controls_layout.addWidget(self.collision_help)

        self.clear_collision_btn = QPushButton("Clear Collision Data")
        self.clear_collision_btn.clicked.connect(self._clear_collision)
        collision_controls_layout.addWidget(self.clear_collision_btn)

        collision_layout.addLayout(collision_controls_layout)

        collision_group.setLayout(collision_layout)
        right_layout.addWidget(collision_group, 1)

        # Tile properties
        properties_scroll = QScrollArea()
        properties_scroll.setWidgetResizable(True)

        self.properties_widget = TilePropertiesWidget()
        self.properties_widget.properties_changed.connect(self._on_properties_changed)
        properties_scroll.setWidget(self.properties_widget)

        right_layout.addWidget(properties_scroll)

        right_widget.setLayout(right_layout)
        main_splitter.addWidget(right_widget)

        main_splitter.setSizes([600, 400])
        main_layout.addWidget(main_splitter, 1)

        main_widget.setLayout(main_layout)
        self.content_layout.addWidget(main_widget)

    def _new_tileset(self):
        """Create a new tileset"""
        if self.tileset_widget.get_tile_count() > 0:
            reply = QMessageBox.question(
                self,
                "Unsaved Changes",
                "Create new tileset? Unsaved changes will be lost.",
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
            )
            if reply == QMessageBox.StandardButton.No:
                return

        self.current_file = None
        self.tileset_path = None
        self.asset_uuid = str(uuid.uuid4())
        self.tileset_widget.tiles.clear()
        self.tileset_widget.tileset_image = None
        self.tileset_widget.update()
        self.collision_widget.set_tile(None, None)
        self.properties_widget.set_tile(None)
        self.collision_type_combo.setCurrentText("None")
        self.collision_help.setText("Select a tile to edit collision")
        self.selected_tile_label.setText("No tile selected")
        self.tile_info_label.setText("No tileset loaded")

    def _load_tileset(self):
        """Load a .tileset2D file"""
        file_path, _ = QFileDialog.getOpenFileName(
            self,
            "Load Tileset",
            str(self._get_tilesets_directory()) if self.project_root else "",
            "Tileset 2D (*.tileset2D)"
        )

        if file_path:
            try:
                with open(file_path, 'r', encoding='utf-8') as f:
                    data = json.load(f)

                self.current_file = file_path
                self.asset_uuid = data.get('uuid', str(uuid.uuid4()))
                self.tileset_path = data.get('tileset_image_path', '')

                # Load tileset image
                tile_width = data.get('tile_width', 32)
                tile_height = data.get('tile_height', 32)

                self.tile_width_spin.blockSignals(True)
                self.tile_height_spin.blockSignals(True)
                self.tile_width_spin.setValue(tile_width)
                self.tile_height_spin.setValue(tile_height)
                self.tile_width_spin.blockSignals(False)
                self.tile_height_spin.blockSignals(False)

                if self.tileset_path and Path(self.tileset_path).exists():
                    self.tileset_widget.load_tileset(self.tileset_path, tile_width, tile_height)

                    # Load tile data
                    tiles_data = data.get('tiles', [])
                    for tile_json in tiles_data:
                        idx = tile_json.get('index', 0)
                        if idx < len(self.tileset_widget.tiles):
                            tile = self.tileset_widget.tiles[idx]
                            tile.tags = tile_json.get('tags', [])
                            tile.metadata = tile_json.get('metadata', {})
                            tile.collision_shape_type = tile_json.get('collision_shape_type', 'None')
                            tile.collision_radius = tile_json.get('collision_radius', 0.5)

                            # Load collision rect
                            rect_data = tile_json.get('collision_rect', [0, 0, 1, 1])
                            tile.collision_rect = QRect(rect_data[0], rect_data[1], rect_data[2], rect_data[3])

                            # Load collision vertices
                            vertices_data = tile_json.get('collision_vertices', [])
                            tile.collision_vertices = [QPointF(v[0], v[1]) for v in vertices_data]

                    self._update_tile_info()
                    self._apply_texture_filtering()

                    QMessageBox.information(self, "Success", "Tileset loaded successfully")
                else:
                    QMessageBox.warning(self, "Error", f"Tileset image not found: {self.tileset_path}")

            except Exception as e:
                QMessageBox.warning(self, "Error", f"Failed to load tileset:\\n{str(e)}")

    def _save_tileset(self):
        """Save the current tileset"""
        if not self.current_file:
            self._save_as_tileset()
            return

        self._do_save(self.current_file)

    def _save_as_tileset(self):
        """Save tileset with a new name"""
        file_path, _ = QFileDialog.getSaveFileName(
            self,
            "Save Tileset",
            str(self._get_tilesets_directory()) if self.project_root else "",
            "Tileset 2D (*.tileset2D)"
        )

        if file_path:
            if not file_path.endswith('.tileset2D'):
                file_path += '.tileset2D'

            self.current_file = file_path
            self._do_save(file_path)

    def _do_save(self, file_path):
        """Perform the save operation"""
        try:
            # Build tileset data
            tiles_data = []
            for tile in self.tileset_widget.tiles:
                tile_json = {
                    'index': tile.index,
                    'tags': tile.tags,
                    'metadata': tile.metadata,
                    'collision_shape_type': tile.collision_shape_type,
                    'collision_radius': tile.collision_radius,
                    'collision_rect': [tile.collision_rect.x(), tile.collision_rect.y(),
                                      tile.collision_rect.width(), tile.collision_rect.height()],
                    'collision_vertices': [[v.x(), v.y()] for v in tile.collision_vertices]
                }
                tiles_data.append(tile_json)

            data = {
                'uuid': self.asset_uuid,
                'tileset_image_path': self.tileset_path,
                'tile_width': self.tileset_widget.tile_width,
                'tile_height': self.tileset_widget.tile_height,
                'tiles': tiles_data
            }

            with open(file_path, 'w', encoding='utf-8') as f:
                json.dump(data, f, indent=2)

            QMessageBox.information(self, "Success", "Tileset saved successfully")
        except Exception as e:
            QMessageBox.warning(self, "Error", f"Failed to save tileset:\\n{str(e)}")

    def _export_json(self):
        """Export tileset data to JSON"""
        file_path, _ = QFileDialog.getSaveFileName(
            self,
            "Export JSON",
            str(self.project_root) if self.project_root else "",
            "JSON Files (*.json)"
        )

        if file_path:
            self._do_save(file_path)

    def _import_json(self):
        """Import tileset data from JSON"""
        file_path, _ = QFileDialog.getOpenFileName(
            self,
            "Import JSON",
            str(self.project_root) if self.project_root else "",
            "JSON Files (*.json)"
        )

        if file_path:
            try:
                with open(file_path, 'r', encoding='utf-8') as f:
                    data = json.load(f)

                # Try to load as tileset2D format
                if 'tiles' in data:
                    self.tileset_path = data.get('tileset_image_path', '')
                    tile_width = data.get('tile_width', 32)
                    tile_height = data.get('tile_height', 32)

                    self.tile_width_spin.setValue(tile_width)
                    self.tile_height_spin.setValue(tile_height)

                    if self.tileset_path:
                        self.tileset_widget.load_tileset(self.tileset_path, tile_width, tile_height)

                        # Load tile data
                        tiles_data = data.get('tiles', [])
                        for tile_json in tiles_data:
                            idx = tile_json.get('index', 0)
                            if idx < len(self.tileset_widget.tiles):
                                tile = self.tileset_widget.tiles[idx]
                                tile.tags = tile_json.get('tags', [])
                                tile.metadata = tile_json.get('metadata', {})
                                tile.collision_shape_type = tile_json.get('collision_shape_type', 'None')
                                tile.collision_radius = tile_json.get('collision_radius', 0.5)
                                rect_data = tile_json.get('collision_rect', [0, 0, 1, 1])
                                tile.collision_rect = QRect(rect_data[0], rect_data[1], rect_data[2], rect_data[3])
                                vertices_data = tile_json.get('collision_vertices', [])
                                tile.collision_vertices = [QPointF(v[0], v[1]) for v in vertices_data]

                        self._update_tile_info()
                        QMessageBox.information(self, "Success", "JSON imported successfully")
                else:
                    QMessageBox.warning(self, "Error", "Invalid tileset JSON format")

            except Exception as e:
                QMessageBox.warning(self, "Error", f"Failed to import JSON:\\n{str(e)}")

    def _load_tileset_image(self):
        """Load a tileset image"""
        file_path, _ = QFileDialog.getOpenFileName(
            self,
            "Load Tileset Image",
            str(self.project_root) if self.project_root else "",
            "Images (*.png *.jpg *.jpeg *.bmp)"
        )

        if file_path:
            self.tileset_path = file_path
            tile_width = self.tile_width_spin.value()
            tile_height = self.tile_height_spin.value()

            success = self.tileset_widget.load_tileset(file_path, tile_width, tile_height)
            if success:
                self._update_tile_info()
                self._apply_texture_filtering()
            else:
                QMessageBox.warning(self, "Error", "Failed to load tileset image")

    def _on_tile_size_changed(self):
        """Handle tile size change"""
        if not self.tileset_path:
            return

        tile_width = self.tile_width_spin.value()
        tile_height = self.tile_height_spin.value()

        # Reload tileset with new tile size
        self.tileset_widget.load_tileset(self.tileset_path, tile_width, tile_height)
        self._update_tile_info()
        self._apply_texture_filtering()

    def _on_scale_changed(self):
        """Handle display scale change"""
        self.tileset_widget.display_scale = self.scale_spin.value()
        self.tileset_widget.update()

    def _on_tile_selected(self, tile_index):
        """Handle tile selection"""
        tile = self.tileset_widget.get_selected_tile()
        if tile:
            self.selected_tile_label.setText(f"Tile {tile_index} selected")
            tile_pixmap = self.tileset_widget.get_tile_pixmap(tile_index)
            self.collision_widget.set_tile(tile, tile_pixmap)
            self.properties_widget.set_tile(tile)

            # Update collision controls
            self.collision_type_combo.blockSignals(True)
            self.collision_type_combo.setCurrentText(tile.collision_shape_type)
            self.collision_type_combo.blockSignals(False)
            self._update_collision_help()

    def _on_collision_type_changed(self, collision_type):
        """Handle collision type change"""
        tile = self.tileset_widget.get_selected_tile()
        if not tile:
            return

        tile.collision_shape_type = collision_type

        # Initialize collision data based on type
        if collision_type == "Rectangle":
            tile.collision_rect = QRect(0, 0, 1, 1)
        elif collision_type == "Circle":
            tile.collision_radius = 0.5
        elif collision_type == "Polygon":
            if not tile.collision_vertices:
                # Start with empty polygon
                tile.collision_vertices = []

        self._update_collision_help()
        self.collision_widget.update()
        self.tileset_widget.update()

    def _update_collision_help(self):
        """Update collision help text"""
        tile = self.tileset_widget.get_selected_tile()
        if not tile:
            self.collision_help.setText("Select a tile to edit collision")
            return

        collision_type = tile.collision_shape_type
        if collision_type == "None":
            self.collision_help.setText("No collision shape")
        elif collision_type == "Rectangle":
            self.collision_help.setText("Rectangle covers the entire tile")
        elif collision_type == "Circle":
            self.collision_help.setText("Circle centered on tile")
        elif collision_type == "Polygon":
            self.collision_help.setText(
                "Click in collision editor to add vertices\n"
                "Right-click vertices to remove them\n"
                "Drag vertices to adjust position"
            )

    def _clear_collision(self):
        """Clear collision data"""
        tile = self.tileset_widget.get_selected_tile()
        if not tile:
            return

        tile.collision_vertices.clear()
        tile.collision_radius = 0.5
        tile.collision_rect = QRect(0, 0, 1, 1)
        self.collision_widget.update()
        self.tileset_widget.update()

    def _on_collision_changed(self):
        """Handle collision shape change"""
        self.tileset_widget.update()

    def _on_properties_changed(self):
        """Handle properties change"""
        self.tileset_widget.update()

    def _update_tile_info(self):
        """Update tile info label"""
        tile_count = self.tileset_widget.get_tile_count()
        self.tile_info_label.setText(f"{tile_count} tiles loaded")

    def _get_tilesets_directory(self):
        """Get the tilesets directory path"""
        if self.project_root:
            tileset_dir = Path(self.project_root) / "assets" / "tilesets"
            tileset_dir.mkdir(parents=True, exist_ok=True)
            return tileset_dir
        return Path.home()
