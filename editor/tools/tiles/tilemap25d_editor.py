"""
TileMap 2.5D Editor Tool
A comprehensive tool for creating 3D maps using 2D tilesets (similar to Crocotile3D)
Supports placing textured faces in 3D space with vertex manipulation
"""

from PyQt6.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QLabel,
                             QPushButton, QListWidget, QListWidgetItem,
                             QFileDialog, QMessageBox, QDialog, QFormLayout,
                             QLineEdit, QComboBox, QDialogButtonBox, QSpinBox,
                             QDoubleSpinBox, QSplitter, QGroupBox, QGridLayout,
                             QScrollArea, QRadioButton, QButtonGroup, QFrame,
                             QCheckBox, QTextEdit, QTabWidget, QColorDialog,
                             QSlider, QToolButton, QMenu, QSizePolicy)
from PyQt6.QtCore import Qt, QTimer, pyqtSignal, QSize, QRect, QPoint, QPointF
from PyQt6.QtGui import (QPixmap, QImage, QPainter, QColor, QPen, QPolygonF,
                         QBrush, QWheelEvent, QMouseEvent, QPainterPath,
                         QPalette, QKeyEvent)
from pathlib import Path
import json
import sys
import uuid
import copy
import math
from typing import List, Optional, Tuple, Dict, Set
from dataclasses import dataclass, field
from enum import Enum

# Import base panel
editor_path = Path(__file__).parent.parent.parent
if str(editor_path) not in sys.path:
    sys.path.insert(0, str(editor_path))

from panels.base_panel import EditorPanel
from theme import get_theme_manager
from native_render_widget import NativeRenderWidget

try:
    import lupine_engine as le
    HAS_ENGINE = True
except ImportError:
    HAS_ENGINE = False


def _resolve_res_path(res_path: str, project_root: str = None) -> str:
    """Resolve a res:// path to an absolute path"""
    if not res_path:
        return res_path

    if res_path.startswith("res://"):
        # Try to get project root from AssetDatabase first
        if not project_root:
            try:
                if HAS_ENGINE:
                    asset_db = le.AssetDatabase.get_instance()
                    if asset_db.is_initialized():
                        project_root = asset_db.get_project_root()
            except Exception:
                pass

        if not project_root:
            # Try panels.inspector_panel
            try:
                from panels.inspector_panel import get_project_root
                project_root = get_project_root()
            except Exception:
                pass

        if project_root:
            relative_path = res_path[6:]  # Remove "res://"
            return str(Path(project_root) / relative_path)

    return res_path


class EditMode(Enum):
    """Edit modes for the 2.5D editor"""
    PLACE = "place"           # Place new tile faces
    SELECT_FACE = "select_face"  # Select and manipulate faces
    SELECT_VERTEX = "select_vertex"  # Select and move vertices
    SELECT_EDGE = "select_edge"  # Select and move edges
    DELETE = "delete"         # Delete faces
    PAINT = "paint"           # Paint existing faces with new tiles


class SnapMode(Enum):
    """Snapping modes for vertex manipulation"""
    NONE = "none"
    GRID = "grid"
    VERTEX = "vertex"
    EDGE = "edge"


class PlaneOrientation(Enum):
    """Orientation of the working plane"""
    FLOOR = "floor"       # XZ plane (horizontal floor)
    WALL_X = "wall_x"     # YZ plane (wall facing X)
    WALL_Z = "wall_z"     # XY plane (wall facing Z)


@dataclass
class Vec3:
    """3D Vector"""
    x: float = 0.0
    y: float = 0.0
    z: float = 0.0

    def __add__(self, other):
        return Vec3(self.x + other.x, self.y + other.y, self.z + other.z)

    def __sub__(self, other):
        return Vec3(self.x - other.x, self.y - other.y, self.z - other.z)

    def __mul__(self, scalar):
        return Vec3(self.x * scalar, self.y * scalar, self.z * scalar)

    def length(self):
        return math.sqrt(self.x**2 + self.y**2 + self.z**2)

    def normalized(self):
        l = self.length()
        if l > 0:
            return Vec3(self.x / l, self.y / l, self.z / l)
        return Vec3()

    def dot(self, other):
        return self.x * other.x + self.y * other.y + self.z * other.z

    def cross(self, other):
        return Vec3(
            self.y * other.z - self.z * other.y,
            self.z * other.x - self.x * other.z,
            self.x * other.y - self.y * other.x
        )

    def to_tuple(self):
        return (self.x, self.y, self.z)

    def copy(self):
        return Vec3(self.x, self.y, self.z)

    def distance_to(self, other):
        return (self - other).length()


@dataclass
class Vec2:
    """2D Vector for UVs"""
    u: float = 0.0
    v: float = 0.0

    def to_tuple(self):
        return (self.u, self.v)


@dataclass
class TileFace:
    """Represents a single tile face in 3D space"""
    id: str = field(default_factory=lambda: str(uuid.uuid4()))
    # Four vertices defining the quad (counter-clockwise)
    vertices: List[Vec3] = field(default_factory=lambda: [Vec3(), Vec3(), Vec3(), Vec3()])
    # UV coordinates for each vertex
    uvs: List[Vec2] = field(default_factory=lambda: [Vec2(0,0), Vec2(1,0), Vec2(1,1), Vec2(0,1)])
    # Tileset reference
    tileset_index: int = 0
    tile_index: int = 0
    # Visual properties
    visible: bool = True
    two_sided: bool = True  # Default to two-sided for proper visibility

    def get_center(self) -> Vec3:
        """Get the center point of the face"""
        cx = sum(v.x for v in self.vertices) / 4
        cy = sum(v.y for v in self.vertices) / 4
        cz = sum(v.z for v in self.vertices) / 4
        return Vec3(cx, cy, cz)

    def get_normal(self) -> Vec3:
        """Calculate the face normal"""
        v0, v1, v2, v3 = self.vertices
        edge1 = v1 - v0
        edge2 = v3 - v0
        normal = edge1.cross(edge2)
        return normal.normalized()

    def copy(self):
        """Create a deep copy of this face"""
        return TileFace(
            id=str(uuid.uuid4()),
            vertices=[v.copy() for v in self.vertices],
            uvs=[Vec2(uv.u, uv.v) for uv in self.uvs],
            tileset_index=self.tileset_index,
            tile_index=self.tile_index,
            visible=self.visible,
            two_sided=self.two_sided
        )


@dataclass
class TilesetData25D:
    """Represents a loaded tileset for 2.5D mapping"""
    file_path: str = ""
    name: str = ""
    image_pixmap: QPixmap = None
    image_path: str = ""  # Store the actual image path for C++ backend
    tile_width: int = 32
    tile_height: int = 32
    columns: int = 0
    rows: int = 0

    def load(self) -> bool:
        """Load tileset from file"""
        try:
            # Resolve res:// path to absolute path for file access
            resolved_file_path = _resolve_res_path(self.file_path)

            with open(resolved_file_path, 'r', encoding='utf-8') as f:
                data = json.load(f)

            self.tile_width = data.get('tile_width', 32)
            self.tile_height = data.get('tile_height', 32)
            tileset_image_path = data.get('tileset_image_path', '')

            # Resolve the tileset image path (may be res:// path)
            resolved_image_path = _resolve_res_path(tileset_image_path)

            if resolved_image_path and Path(resolved_image_path).exists():
                self.image_path = resolved_image_path  # Store resolved path for C++ backend
                self.image_pixmap = QPixmap(self.image_path)
                self.columns = self.image_pixmap.width() // self.tile_width
                self.rows = self.image_pixmap.height() // self.tile_height
                self.name = Path(resolved_file_path).stem
                return True
        except Exception as e:
            print(f"Failed to load tileset: {e}")
        return False

    def get_tile_count(self) -> int:
        """Get total number of tiles"""
        return self.columns * self.rows

    def get_tile_pixmap(self, tile_index: int) -> QPixmap:
        """Get pixmap for a specific tile"""
        if not self.image_pixmap or tile_index < 0:
            return QPixmap()

        row = tile_index // self.columns
        col = tile_index % self.columns

        x = col * self.tile_width
        y = row * self.tile_height

        return self.image_pixmap.copy(x, y, self.tile_width, self.tile_height)

    def get_tile_uv_rect(self, tile_index: int) -> Tuple[float, float, float, float]:
        """Get normalized UV coordinates for a tile (u_min, v_min, u_max, v_max)

        Note: The texture is flipped vertically when loaded (stbi_set_flip_vertically_on_load),
        so we need to invert the V coordinates to get the correct tile.
        """
        if not self.image_pixmap or tile_index < 0:
            return (0, 0, 1, 1)

        row = tile_index // self.columns
        col = tile_index % self.columns

        img_w = self.image_pixmap.width()
        img_h = self.image_pixmap.height()

        u_min = col * self.tile_width / img_w
        u_max = (col + 1) * self.tile_width / img_w

        # Calculate V in original image coordinates
        v_top_orig = row * self.tile_height / img_h
        v_bottom_orig = (row + 1) * self.tile_height / img_h

        # Flip V because texture is loaded with vertical flip
        # Original top (lower V) becomes bottom (lower V) after flip
        v_min = 1.0 - v_bottom_orig  # Top of tile in flipped texture
        v_max = 1.0 - v_top_orig     # Bottom of tile in flipped texture

        return (u_min, v_min, u_max, v_max)


class TileMap25DDocument:
    """Document containing all tile faces and tilesets"""

    def __init__(self):
        self.faces: List[TileFace] = []
        self.tilesets: List[TilesetData25D] = []
        self.name: str = "Untitled"
        self.grid_size: float = 1.0

    def add_face(self, face: TileFace):
        """Add a face to the document"""
        self.faces.append(face)

    def remove_face(self, face_id: str):
        """Remove a face by ID"""
        self.faces = [f for f in self.faces if f.id != face_id]

    def get_face_by_id(self, face_id: str) -> Optional[TileFace]:
        """Get a face by its ID"""
        for face in self.faces:
            if face.id == face_id:
                return face
        return None

    def clear(self):
        """Clear all faces"""
        self.faces.clear()

    def to_dict(self) -> dict:
        """Convert to dictionary for serialization"""
        return {
            'name': self.name,
            'grid_size': self.grid_size,
            'tilesets': [ts.file_path for ts in self.tilesets],
            'faces': [
                {
                    'id': f.id,
                    'vertices': [[v.x, v.y, v.z] for v in f.vertices],
                    'uvs': [[uv.u, uv.v] for uv in f.uvs],
                    'tileset_index': f.tileset_index,
                    'tile_index': f.tile_index,
                    'visible': f.visible,
                    'two_sided': f.two_sided
                }
                for f in self.faces
            ]
        }

    def from_dict(self, data: dict, load_tilesets: bool = True):
        """Load from dictionary"""
        self.name = data.get('name', 'Untitled')
        self.grid_size = data.get('grid_size', 1.0)

        # Load tilesets
        if load_tilesets:
            self.tilesets.clear()
            for ts_path in data.get('tilesets', []):
                # Resolve res:// path to absolute path for existence check
                resolved_path = _resolve_res_path(ts_path)
                if Path(resolved_path).exists():
                    # Keep original res:// path for storage
                    ts = TilesetData25D(file_path=ts_path)
                    if ts.load():
                        self.tilesets.append(ts)

        # Load faces
        self.faces.clear()
        for f_data in data.get('faces', []):
            face = TileFace(
                id=f_data.get('id', str(uuid.uuid4())),
                vertices=[Vec3(*v) for v in f_data.get('vertices', [[0,0,0]]*4)],
                uvs=[Vec2(*uv) for uv in f_data.get('uvs', [[0,0],[1,0],[1,1],[0,1]])],
                tileset_index=f_data.get('tileset_index', 0),
                tile_index=f_data.get('tile_index', 0),
                visible=f_data.get('visible', True),
                two_sided=f_data.get('two_sided', True)
            )
            self.faces.append(face)


class TilePalette25DWidget(QWidget):
    """Widget for displaying and selecting tiles from loaded tilesets"""

    tile_selected = pyqtSignal(int, int)  # tileset_index, tile_index

    def __init__(self, parent=None):
        super().__init__(parent)
        self.tilesets: List[TilesetData25D] = []
        self.selected_tileset_index = -1
        self.selected_tile_index = -1
        self.hover_tileset_index = -1
        self.hover_tile_index = -1
        self.display_scale = 2.0

        self.setMinimumSize(200, 300)
        self.setMouseTracking(True)

    def add_tileset(self, tileset: TilesetData25D):
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
            painter.drawText(self.rect(), Qt.AlignmentFlag.AlignCenter, "Load a tileset")
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
            display_tile_width = int(tileset.tile_width * self.display_scale)
            display_tile_height = int(tileset.tile_height * self.display_scale)

            # Calculate how many columns fit
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
                    scaled = tile_pixmap.scaled(
                        display_tile_width, display_tile_height,
                        Qt.AspectRatioMode.IgnoreAspectRatio,
                        Qt.TransformationMode.FastTransformation
                    )
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

    def _get_tile_at_pos(self, pos) -> Tuple[int, int]:
        """Get tileset and tile index at position"""
        y_offset = 5
        padding = 4

        for tileset_idx, tileset in enumerate(self.tilesets):
            if not tileset.image_pixmap:
                continue

            y_offset += 25  # Skip header

            tile_count = tileset.get_tile_count()
            display_tile_width = int(tileset.tile_width * self.display_scale)
            display_tile_height = int(tileset.tile_height * self.display_scale)

            available_width = self.width() - padding * 2
            tiles_per_row = max(1, available_width // (display_tile_width + padding))

            rows = (tile_count + tiles_per_row - 1) // tiles_per_row
            tileset_height = rows * (display_tile_height + padding)

            if y_offset <= pos.y() <= y_offset + tileset_height:
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


class TileMap25DViewport(QWidget):
    """3D Viewport for the 2.5D tilemap editor"""

    face_placed = pyqtSignal()
    selection_changed = pyqtSignal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setMinimumSize(400, 300)
        self.setFocusPolicy(Qt.FocusPolicy.StrongFocus)

        # Set background color
        palette = self.palette()
        palette.setColor(QPalette.ColorRole.Window, QColor(30, 30, 30))
        self.setAutoFillBackground(True)
        self.setPalette(palette)

        # Render widget. The engine renders into this widget's native window
        # handle, so it must be a NativeRenderWidget that Qt never paints/erases;
        # a plain background-filled QWidget here flashes to its fill colour for one
        # Qt frame on every resize/repaint before the engine redraws (flicker).
        self.render_widget = NativeRenderWidget(self, surface_color=(30, 30, 30))
        self.render_widget.setMinimumSize(400, 300)
        self.render_widget.setMouseTracking(True)
        self.render_widget.setFocusPolicy(Qt.FocusPolicy.StrongFocus)
        self.render_widget.installEventFilter(self)

        layout = QVBoxLayout()
        layout.setContentsMargins(0, 0, 0, 0)
        layout.addWidget(self.render_widget, stretch=1)
        self.setLayout(layout)

        # Engine integration
        self.editor_bridge = None
        self.view_id = None
        self.tilemap_renderer_id = None

        # Document
        self.document: Optional[TileMap25DDocument] = None

        # Render timer
        self.render_timer = None

        # Mouse state
        self.last_mouse_pos = QPoint()
        self.middle_mouse_down = False
        self.middle_mouse_start_pos = QPoint()  # Track start for tap detection
        self.middle_mouse_dragged = False  # True if dragged significantly
        self.right_mouse_down = False
        self.left_mouse_down = False
        self.is_dragging = False

        # Edit state
        self.edit_mode = EditMode.PLACE
        self.snap_mode = SnapMode.GRID
        self.plane_orientation = PlaneOrientation.FLOOR

        # Selection
        self.selected_face_ids: Set[str] = set()
        self.selected_vertex_indices: List[Tuple[str, int]] = []  # (face_id, vertex_index)
        self.selected_edge_indices: List[Tuple[str, int]] = []  # (face_id, edge_index 0-3)

        # Tile selection
        self.selected_tileset_index = -1
        self.selected_tile_index = -1

        # Working plane
        self.floor_height = 0.0
        self.grid_size = 1.0
        self.plane_rotation = 0  # 0, 90, 180, 270 degrees

        # Ghost preview
        self.ghost_face: Optional[TileFace] = None
        self.cursor_world_pos = Vec3()
        self.show_ghost = True
        self.show_grid = True

        # Vertex/edge dragging state
        self.dragging_vertex = False
        self.dragging_edge = False
        self.drag_start_pos = Vec3()

        # Face rotation state (for Tab rotation around Y axis)
        self.face_rotation = 0  # 0, 90, 180, 270 degrees around Y

        # Undo/redo stacks
        self.undo_stack: List[dict] = []
        self.redo_stack: List[dict] = []
        self.max_undo_levels = 50

        # Parent reference
        self.parent_tool = None

    def set_document(self, doc: TileMap25DDocument):
        """Set the document to edit"""
        self.document = doc
        # Sync with C++ backend if available
        self._sync_document_to_cpp()

    def _sync_document_to_cpp(self):
        """Synchronize Python document to C++ backend"""
        if not self.editor_bridge or not self.tilemap_renderer_id or not self.document:
            return

        try:
            # Clear C++ backend
            self.editor_bridge.tilemap25d_clear(self.tilemap_renderer_id)

            # Add all faces from document
            for face in self.document.faces:
                v = face.vertices
                uv = face.uvs
                self.editor_bridge.tilemap25d_add_face(
                    self.tilemap_renderer_id,
                    v[0].x, v[0].y, v[0].z,
                    v[1].x, v[1].y, v[1].z,
                    v[2].x, v[2].y, v[2].z,
                    v[3].x, v[3].y, v[3].z,
                    uv[0].u, uv[0].v,
                    uv[1].u, uv[1].v,
                    uv[2].u, uv[2].v,
                    uv[3].u, uv[3].v,
                    face.tileset_index, face.tile_index,
                    face.two_sided
                )
        except Exception as e:
            print(f"[TileMap25DViewport] Error syncing document to C++: {e}")

    def initialize_rendering(self, editor_bridge, editor_session):
        """Initialize 3D rendering"""
        if not HAS_ENGINE:
            return

        self.editor_bridge = editor_bridge

        # Create C++ TileMap25D builder
        try:
            self.tilemap_renderer_id = self.editor_bridge.create_tilemap25d_builder()
        except Exception as e:
            print(f"[TileMap25DViewport] Could not create C++ builder: {e}")
            self.tilemap_renderer_id = None

        window_handle = int(self.render_widget.winId())
        dpr = self.render_widget.devicePixelRatio()
        width = int(self.render_widget.width() * dpr)
        height = int(self.render_widget.height() * dpr)

        # Create scene for the viewport
        scene_doc = editor_session.create_scene("TileMap25D Viewport")
        if scene_doc:
            scene = scene_doc.get_scene()

            # Create render view
            self.view_id = self.editor_bridge.create_render_view(window_handle, width, height)

            if self.view_id:
                self.editor_bridge.set_view_mode(self.view_id, le.ViewMode.View3D)
                self.editor_bridge.set_view_scene(self.view_id, scene)

                # Enable debug rendering for selection visualization
                self.editor_bridge.set_view_debug_rendering_enabled(self.view_id, True)

                # Start render timer
                self.render_timer = QTimer(self)
                self.render_timer.timeout.connect(self.render_frame)
                self.render_timer.start(16)  # ~60 FPS

    def render_frame(self):
        """Render the current frame"""
        if not self.editor_bridge or not self.view_id:
            return

        try:
            # Render using C++ backend if available
            if self.tilemap_renderer_id:
                self.editor_bridge.tilemap25d_render(self.tilemap_renderer_id, self.view_id)
            else:
                # Fallback to debug line rendering
                self._render_faces()

            # Render ghost preview
            if self.show_ghost and self.ghost_face and self.edit_mode == EditMode.PLACE:
                self._render_ghost_face()

            # Render selection highlights
            self._render_selection()

            # Render floor grid
            if self.show_grid:
                self._render_floor_grid()

            # Render the view
            self.editor_bridge.render_view(self.view_id)
            self.render_widget.notify_rendered()
        except Exception as e:
            print(f"[TileMap25DViewport] Error rendering: {e}")

    def _render_faces(self):
        """Render all faces in the document"""
        if not self.document or not self.editor_bridge or not self.view_id:
            return

        for face in self.document.faces:
            if not face.visible:
                continue

            # Get tileset for this face
            if face.tileset_index < len(self.document.tilesets):
                tileset = self.document.tilesets[face.tileset_index]
                # Render textured quad using debug renderer or custom renderer
                self._render_textured_quad(face, tileset)

    def _render_textured_quad(self, face: TileFace, tileset: TilesetData25D):
        """Render a textured quad face"""
        if not self.editor_bridge or not self.view_id:
            return

        # For now, render as colored quad using debug lines
        # In a full implementation, this would use a proper mesh renderer
        v = face.vertices

        # Determine color based on selection state
        if face.id in self.selected_face_ids:
            r, g, b, a = 0.2, 0.8, 1.0, 1.0  # Cyan for selected
        else:
            r, g, b, a = 0.8, 0.8, 0.8, 1.0  # Gray for normal

        try:
            # Draw quad edges
            self.editor_bridge.debug_draw_line(
                self.view_id,
                v[0].x, v[0].y, v[0].z, v[1].x, v[1].y, v[1].z,
                r, g, b, a
            )
            self.editor_bridge.debug_draw_line(
                self.view_id,
                v[1].x, v[1].y, v[1].z, v[2].x, v[2].y, v[2].z,
                r, g, b, a
            )
            self.editor_bridge.debug_draw_line(
                self.view_id,
                v[2].x, v[2].y, v[2].z, v[3].x, v[3].y, v[3].z,
                r, g, b, a
            )
            self.editor_bridge.debug_draw_line(
                self.view_id,
                v[3].x, v[3].y, v[3].z, v[0].x, v[0].y, v[0].z,
                r, g, b, a
            )

            # Draw diagonal for visual fill
            self.editor_bridge.debug_draw_line(
                self.view_id,
                v[0].x, v[0].y, v[0].z, v[2].x, v[2].y, v[2].z,
                r * 0.5, g * 0.5, b * 0.5, a * 0.5
            )
        except:
            pass

    def _render_ghost_face(self):
        """Render the ghost preview face"""
        if not self.ghost_face or not self.editor_bridge or not self.view_id:
            return

        v = self.ghost_face.vertices
        r, g, b, a = 0.3, 0.6, 1.0, 0.5  # Blue transparent

        try:
            # Draw ghost quad edges
            self.editor_bridge.debug_draw_line(
                self.view_id,
                v[0].x, v[0].y, v[0].z, v[1].x, v[1].y, v[1].z,
                r, g, b, a
            )
            self.editor_bridge.debug_draw_line(
                self.view_id,
                v[1].x, v[1].y, v[1].z, v[2].x, v[2].y, v[2].z,
                r, g, b, a
            )
            self.editor_bridge.debug_draw_line(
                self.view_id,
                v[2].x, v[2].y, v[2].z, v[3].x, v[3].y, v[3].z,
                r, g, b, a
            )
            self.editor_bridge.debug_draw_line(
                self.view_id,
                v[3].x, v[3].y, v[3].z, v[0].x, v[0].y, v[0].z,
                r, g, b, a
            )

            # Fill diagonal
            self.editor_bridge.debug_draw_line(
                self.view_id,
                v[0].x, v[0].y, v[0].z, v[2].x, v[2].y, v[2].z,
                r, g, b, a * 0.3
            )
        except:
            pass

    def _render_selection(self):
        """Render selection highlights - vertices, edges, and faces"""
        if not self.document or not self.editor_bridge or not self.view_id:
            return

        try:
            # Render edges and vertices for all selected faces
            for face_id in self.selected_face_ids:
                face = self.document.get_face_by_id(face_id)
                if not face:
                    continue

                # Draw face edges in cyan (no depth test so they show on top)
                for i in range(4):
                    v1 = face.vertices[i]
                    v2 = face.vertices[(i + 1) % 4]
                    self.editor_bridge.debug_draw_line(
                        self.view_id,
                        v1.x, v1.y, v1.z,
                        v2.x, v2.y, v2.z,
                        0.0, 0.9, 1.0, 1.0,  # Cyan for edges
                        False  # No depth test - always show on top
                    )

                # Draw vertices as small crosses (green for unselected)
                for i, v in enumerate(face.vertices):
                    is_selected = (face_id, i) in self.selected_vertex_indices
                    if is_selected:
                        continue  # Draw selected vertices later (on top)

                    size = 0.08
                    # Green for unselected vertices
                    self.editor_bridge.debug_draw_line(
                        self.view_id,
                        v.x - size, v.y, v.z, v.x + size, v.y, v.z,
                        0.2, 1.0, 0.2, 1.0, False
                    )
                    self.editor_bridge.debug_draw_line(
                        self.view_id,
                        v.x, v.y - size, v.z, v.x, v.y + size, v.z,
                        0.2, 1.0, 0.2, 1.0, False
                    )
                    self.editor_bridge.debug_draw_line(
                        self.view_id,
                        v.x, v.y, v.z - size, v.x, v.y, v.z + size,
                        0.2, 1.0, 0.2, 1.0, False
                    )

            # Render selected vertices on top (larger, yellow)
            for face_id, vertex_idx in self.selected_vertex_indices:
                face = self.document.get_face_by_id(face_id)
                if face and 0 <= vertex_idx < len(face.vertices):
                    v = face.vertices[vertex_idx]
                    size = 0.12  # Larger for selected
                    # Yellow for selected vertices
                    self.editor_bridge.debug_draw_line(
                        self.view_id,
                        v.x - size, v.y, v.z, v.x + size, v.y, v.z,
                        1.0, 1.0, 0.0, 1.0, False
                    )
                    self.editor_bridge.debug_draw_line(
                        self.view_id,
                        v.x, v.y - size, v.z, v.x, v.y + size, v.z,
                        1.0, 1.0, 0.0, 1.0, False
                    )
                    self.editor_bridge.debug_draw_line(
                        self.view_id,
                        v.x, v.y, v.z - size, v.x, v.y, v.z + size,
                        1.0, 1.0, 0.0, 1.0, False
                    )

            # Render selected edges (orange/yellow highlight)
            for face_id, edge_idx in self.selected_edge_indices:
                face = self.document.get_face_by_id(face_id)
                if face:
                    v1 = face.vertices[edge_idx]
                    v2 = face.vertices[(edge_idx + 1) % 4]
                    # Draw main edge in orange
                    self.editor_bridge.debug_draw_line(
                        self.view_id,
                        v1.x, v1.y, v1.z, v2.x, v2.y, v2.z,
                        1.0, 0.6, 0.0, 1.0, False  # Orange for selected edges
                    )
                    # Draw highlight vertices at edge endpoints
                    for v in [v1, v2]:
                        size = 0.1
                        self.editor_bridge.debug_draw_line(
                            self.view_id,
                            v.x - size, v.y, v.z, v.x + size, v.y, v.z,
                            1.0, 0.8, 0.0, 1.0, False  # Yellow-orange
                        )
                        self.editor_bridge.debug_draw_line(
                            self.view_id,
                            v.x, v.y - size, v.z, v.x, v.y + size, v.z,
                            1.0, 0.8, 0.0, 1.0, False
                        )
                        self.editor_bridge.debug_draw_line(
                            self.view_id,
                            v.x, v.y, v.z - size, v.x, v.y, v.z + size,
                            1.0, 0.8, 0.0, 1.0, False
                        )

            # In SELECT_VERTEX mode, also show vertices for faces near cursor
            if self.edit_mode == EditMode.SELECT_VERTEX and self.cursor_world_pos:
                hover_threshold = self.grid_size * 0.75  # Match selection threshold
                for face in self.document.faces:
                    if face.id in self.selected_face_ids:
                        continue  # Already drawn above
                    # Check if any vertex is near cursor
                    for i, v in enumerate(face.vertices):
                        dist = (Vec3(v.x, v.y, v.z) - self.cursor_world_pos).length()
                        if dist < hover_threshold:  # Highlight nearby vertices
                            size = 0.08
                            self.editor_bridge.debug_draw_line(
                                self.view_id,
                                v.x - size, v.y, v.z, v.x + size, v.y, v.z,
                                0.5, 0.5, 1.0, 0.8, False  # Light blue for hoverable
                            )
                            self.editor_bridge.debug_draw_line(
                                self.view_id,
                                v.x, v.y - size, v.z, v.x, v.y + size, v.z,
                                0.5, 0.5, 1.0, 0.8, False
                            )
                            self.editor_bridge.debug_draw_line(
                                self.view_id,
                                v.x, v.y, v.z - size, v.x, v.y, v.z + size,
                                0.5, 0.5, 1.0, 0.8, False
                            )

            # In SELECT_EDGE mode, highlight edges near cursor
            if self.edit_mode == EditMode.SELECT_EDGE and self.cursor_world_pos:
                hover_threshold = self.grid_size * 0.5  # Match selection threshold
                for face in self.document.faces:
                    for i in range(4):
                        v1 = face.vertices[i]
                        v2 = face.vertices[(i + 1) % 4]
                        dist = self._point_to_line_distance(self.cursor_world_pos, v1, v2)
                        if dist < hover_threshold:
                            # Highlight hoverable edge in light purple
                            self.editor_bridge.debug_draw_line(
                                self.view_id,
                                v1.x, v1.y, v1.z, v2.x, v2.y, v2.z,
                                0.8, 0.5, 1.0, 0.8, False  # Light purple for hoverable
                            )
        except Exception as e:
            pass  # Silently handle rendering errors

    def _render_floor_grid(self):
        """Render the working plane grid"""
        if not self.editor_bridge or not self.view_id:
            return

        # Hide grid when edge snap is enabled - user wants to snap to faces only
        if self.snap_mode == SnapMode.EDGE:
            return

        grid_extent = 10
        step = self.grid_size
        floor_y = self.floor_height

        try:
            if self.plane_orientation == PlaneOrientation.FLOOR:
                # XZ plane grid
                for i in range(-grid_extent, grid_extent + 1):
                    x = i * step
                    # Z lines
                    self.editor_bridge.debug_draw_line(
                        self.view_id,
                        x, floor_y, -grid_extent * step,
                        x, floor_y, grid_extent * step,
                        0.3, 0.3, 0.3, 0.5
                    )
                    # X lines
                    self.editor_bridge.debug_draw_line(
                        self.view_id,
                        -grid_extent * step, floor_y, x,
                        grid_extent * step, floor_y, x,
                        0.3, 0.3, 0.3, 0.5
                    )

            elif self.plane_orientation == PlaneOrientation.WALL_X:
                # YZ plane grid
                for i in range(-grid_extent, grid_extent + 1):
                    coord = i * step
                    # Y lines
                    self.editor_bridge.debug_draw_line(
                        self.view_id,
                        floor_y, coord, -grid_extent * step,
                        floor_y, coord, grid_extent * step,
                        0.3, 0.3, 0.3, 0.5
                    )
                    # Z lines
                    self.editor_bridge.debug_draw_line(
                        self.view_id,
                        floor_y, -grid_extent * step, coord,
                        floor_y, grid_extent * step, coord,
                        0.3, 0.3, 0.3, 0.5
                    )

            elif self.plane_orientation == PlaneOrientation.WALL_Z:
                # XY plane grid
                for i in range(-grid_extent, grid_extent + 1):
                    coord = i * step
                    # X lines
                    self.editor_bridge.debug_draw_line(
                        self.view_id,
                        coord, -grid_extent * step, floor_y,
                        coord, grid_extent * step, floor_y,
                        0.3, 0.3, 0.3, 0.5
                    )
                    # Y lines
                    self.editor_bridge.debug_draw_line(
                        self.view_id,
                        -grid_extent * step, coord, floor_y,
                        grid_extent * step, coord, floor_y,
                        0.3, 0.3, 0.3, 0.5
                    )
        except:
            pass

    def resizeEvent(self, event):
        super().resizeEvent(event)
        if self.editor_bridge and self.view_id:
            dpr = self.render_widget.devicePixelRatio()
            width = int(self.render_widget.width() * dpr)
            height = int(self.render_widget.height() * dpr)
            self.editor_bridge.resize_render_view(self.view_id, width, height)

    def eventFilter(self, obj, event):
        if obj == self.render_widget:
            if event.type() == event.Type.MouseButtonPress:
                self._handle_mouse_press(event)
                return True
            elif event.type() == event.Type.MouseButtonRelease:
                self._handle_mouse_release(event)
                return True
            elif event.type() == event.Type.MouseMove:
                self._handle_mouse_move(event)
                return True
            elif event.type() == event.Type.Wheel:
                self._handle_wheel(event)
                return True
            elif event.type() == event.Type.KeyPress:
                self._handle_key_press(event)
                return True
        return super().eventFilter(obj, event)

    def _handle_mouse_press(self, event: QMouseEvent):
        self.last_mouse_pos = event.pos()

        # IMPORTANT: Update cursor position BEFORE any action that depends on it
        self._update_cursor_position(event.pos())

        if event.button() == Qt.MouseButton.LeftButton:
            self.left_mouse_down = True

            if self.edit_mode == EditMode.PLACE:
                self._place_face()
            elif self.edit_mode == EditMode.SELECT_FACE:
                self._select_face_at_cursor()
            elif self.edit_mode == EditMode.SELECT_VERTEX:
                self._select_vertex_at_cursor()
                if self.selected_vertex_indices:
                    self._save_undo_state()  # Save before starting drag
                    self.dragging_vertex = True
                    self.drag_start_pos = self.cursor_world_pos.copy()
            elif self.edit_mode == EditMode.SELECT_EDGE:
                self._select_edge_at_cursor()
                if self.selected_edge_indices:
                    self._save_undo_state()  # Save before starting drag
                    self.dragging_edge = True
                    self.drag_start_pos = self.cursor_world_pos.copy()
            elif self.edit_mode == EditMode.DELETE:
                self._delete_face_at_cursor()
            elif self.edit_mode == EditMode.PAINT:
                self._paint_face_at_cursor()

        elif event.button() == Qt.MouseButton.MiddleButton:
            self.middle_mouse_down = True
            self.middle_mouse_start_pos = event.pos()
            self.middle_mouse_dragged = False
        elif event.button() == Qt.MouseButton.RightButton:
            self.right_mouse_down = True

    def _handle_mouse_release(self, event: QMouseEvent):
        if event.button() == Qt.MouseButton.LeftButton:
            self.left_mouse_down = False
            self.dragging_vertex = False
            self.dragging_edge = False
        elif event.button() == Qt.MouseButton.MiddleButton:
            # If middle click was a tap (not a drag), flip the tile orientation
            if not self.middle_mouse_dragged:
                self._flip_tile_orientation()
            self.middle_mouse_down = False
            self.middle_mouse_dragged = False
        elif event.button() == Qt.MouseButton.RightButton:
            self.right_mouse_down = False

    def _handle_mouse_move(self, event: QMouseEvent):
        if not self.editor_bridge or not self.view_id:
            return

        current_pos = event.pos()
        delta_x = current_pos.x() - self.last_mouse_pos.x()
        delta_y = current_pos.y() - self.last_mouse_pos.y()

        # Update cursor world position
        self._update_cursor_position(current_pos)

        # Update ghost preview
        self._update_ghost_face()

        # Camera controls
        if self.middle_mouse_down:
            # Mark as dragged if mouse moved significantly (5+ pixels)
            if not self.middle_mouse_dragged:
                drag_dist = (current_pos - self.middle_mouse_start_pos).manhattanLength()
                if drag_dist > 5:
                    self.middle_mouse_dragged = True
            if self.middle_mouse_dragged:
                self.editor_bridge.pan_camera_3d(self.view_id, float(delta_x), float(delta_y))
        elif self.right_mouse_down:
            self.editor_bridge.orbit_camera_3d(self.view_id, float(delta_x), float(delta_y))
        elif self.dragging_vertex and self.left_mouse_down:
            self._drag_selected_vertices()
        elif self.dragging_edge and self.left_mouse_down:
            self._drag_selected_edges()

        self.last_mouse_pos = current_pos

    def _handle_wheel(self, event: QWheelEvent):
        if not self.editor_bridge or not self.view_id:
            return

        delta = event.angleDelta().y() / 120.0
        self.editor_bridge.zoom_camera_3d(self.view_id, delta)

    def _handle_key_press(self, event: QKeyEvent):
        key = event.key()

        # Q - Lower floor
        if key == Qt.Key.Key_Q:
            self.floor_height -= self.grid_size
            if self.parent_tool and hasattr(self.parent_tool, 'floor_spin'):
                self.parent_tool.floor_spin.setValue(self.floor_height)

        # E - Raise floor
        elif key == Qt.Key.Key_E:
            self.floor_height += self.grid_size
            if self.parent_tool and hasattr(self.parent_tool, 'floor_spin'):
                self.parent_tool.floor_spin.setValue(self.floor_height)

        # Tab - Rotate face around Y axis (UV rotation)
        elif key == Qt.Key.Key_Tab:
            self.face_rotation = (self.face_rotation + 90) % 360
            self._update_ghost_face()
            print(f"[TileMap25D] Face rotation: {self.face_rotation}")

        # R - Cycle through plane orientations (Floor -> Wall X -> Wall Z)
        elif key == Qt.Key.Key_R:
            if self.plane_orientation == PlaneOrientation.FLOOR:
                self.plane_orientation = PlaneOrientation.WALL_Z
            elif self.plane_orientation == PlaneOrientation.WALL_Z:
                self.plane_orientation = PlaneOrientation.WALL_X
            else:
                self.plane_orientation = PlaneOrientation.FLOOR
            # Update the combo box in the UI if available
            if self.parent_tool and hasattr(self.parent_tool, 'plane_combo'):
                if self.plane_orientation == PlaneOrientation.FLOOR:
                    self.parent_tool.plane_combo.setCurrentIndex(0)
                elif self.plane_orientation == PlaneOrientation.WALL_X:
                    self.parent_tool.plane_combo.setCurrentIndex(1)
                else:
                    self.parent_tool.plane_combo.setCurrentIndex(2)
            self._update_ghost_face()
            print(f"[TileMap25D] Plane orientation: {self.plane_orientation.value}")

        # Ctrl+Z - Undo
        elif key == Qt.Key.Key_Z and event.modifiers() & Qt.KeyboardModifier.ControlModifier:
            if event.modifiers() & Qt.KeyboardModifier.ShiftModifier:
                self._redo()
            else:
                self._undo()

        # Ctrl+Y - Redo
        elif key == Qt.Key.Key_Y and event.modifiers() & Qt.KeyboardModifier.ControlModifier:
            self._redo()

        # Delete - Delete selected
        elif key == Qt.Key.Key_Delete:
            self._delete_selected()

        # Ctrl+A - Select all
        elif key == Qt.Key.Key_A and event.modifiers() & Qt.KeyboardModifier.ControlModifier:
            self._select_all()

        # Escape - Clear selection
        elif key == Qt.Key.Key_Escape:
            self.selected_face_ids.clear()
            self.selected_vertex_indices.clear()
            self.selected_edge_indices.clear()
            self.selection_changed.emit()

    def _update_cursor_position(self, mouse_pos):
        """Update cursor world position based on mouse using proper ray-plane intersection"""
        if not self.editor_bridge or not self.view_id:
            return

        dpr = self.render_widget.devicePixelRatio()
        screen_x = mouse_pos.x() * dpr
        screen_y = mouse_pos.y() * dpr

        try:
            plane_coord = float(self.floor_height)

            # Define plane normal and point based on orientation
            if self.plane_orientation == PlaneOrientation.FLOOR:
                # XZ plane (horizontal floor), normal pointing up (0, 1, 0)
                plane_normal = (0.0, 1.0, 0.0)
                plane_point = (0.0, plane_coord, 0.0)
            elif self.plane_orientation == PlaneOrientation.WALL_X:
                # YZ plane (wall facing X), normal pointing along X (1, 0, 0)
                plane_normal = (1.0, 0.0, 0.0)
                plane_point = (plane_coord, 0.0, 0.0)
            else:  # WALL_Z
                # XY plane (wall facing Z), normal pointing along Z (0, 0, 1)
                plane_normal = (0.0, 0.0, 1.0)
                plane_point = (0.0, 0.0, plane_coord)

            # Try using the new screen_to_world_on_plane function if available
            world_pos = None
            try:
                world_pos = self.editor_bridge.screen_to_world_on_plane(
                    self.view_id,
                    float(screen_x), float(screen_y),
                    plane_normal[0], plane_normal[1], plane_normal[2],
                    plane_point[0], plane_point[1], plane_point[2]
                )
            except AttributeError:
                # Fallback to old method if new function not available
                world_pos = self.editor_bridge.screen_to_world_3d(
                    self.view_id, float(screen_x), float(screen_y), plane_coord
                )

            if world_pos and isinstance(world_pos, dict):
                wx = world_pos.get('x', 0)
                wy = world_pos.get('y', 0)
                wz = world_pos.get('z', 0)

                # Check for invalid intersection (0,0,0 means no intersection)
                if abs(wx) < 0.0001 and abs(wy) < 0.0001 and abs(wz) < 0.0001:
                    # Keep previous position if no valid intersection
                    return

                # Apply snapping based on mode
                if self.snap_mode == SnapMode.GRID:
                    wx = round(wx / self.grid_size) * self.grid_size
                    wy = round(wy / self.grid_size) * self.grid_size
                    wz = round(wz / self.grid_size) * self.grid_size
                elif self.snap_mode == SnapMode.VERTEX:
                    # Snap to nearest vertex
                    nearest = self._find_nearest_vertex(Vec3(wx, wy, wz))
                    if nearest and nearest.distance_to(Vec3(wx, wy, wz)) < self.grid_size * 0.75:
                        wx, wy, wz = nearest.x, nearest.y, nearest.z
                elif self.snap_mode == SnapMode.EDGE:
                    # Snap to nearest edge point - prioritize edge snapping over grid
                    nearest = self._find_nearest_edge_point(Vec3(wx, wy, wz))
                    if nearest and nearest.distance_to(Vec3(wx, wy, wz)) < self.grid_size * 1.5:
                        wx, wy, wz = nearest.x, nearest.y, nearest.z

                self.cursor_world_pos = Vec3(wx, wy, wz)
        except Exception as e:
            pass

    def _find_nearest_vertex(self, pos: Vec3) -> Optional[Vec3]:
        """Find the nearest vertex to the given position"""
        if not self.document:
            return None

        nearest = None
        min_dist = float('inf')

        for face in self.document.faces:
            for v in face.vertices:
                dist = pos.distance_to(v)
                if dist < min_dist:
                    min_dist = dist
                    nearest = v

        return nearest

    def _find_nearest_edge_point(self, pos: Vec3) -> Optional[Vec3]:
        """Find the nearest point on any edge, preferring edge endpoints (vertices)"""
        if not self.document:
            return None

        nearest = None
        min_dist = float('inf')
        snap_threshold = self.grid_size * 2.0  # Maximum distance to consider

        for face in self.document.faces:
            for i in range(4):
                v0 = face.vertices[i]
                v1 = face.vertices[(i + 1) % 4]

                # First check if we're close to either endpoint (prefer snapping to vertices)
                dist_to_v0 = pos.distance_to(v0)
                dist_to_v1 = pos.distance_to(v1)

                if dist_to_v0 < min_dist and dist_to_v0 < snap_threshold:
                    min_dist = dist_to_v0
                    nearest = v0.copy()

                if dist_to_v1 < min_dist and dist_to_v1 < snap_threshold:
                    min_dist = dist_to_v1
                    nearest = v1.copy()

                # Project point onto edge
                edge = v1 - v0
                edge_len = edge.length()
                if edge_len < 0.0001:
                    continue

                to_point = pos - v0
                t = max(0, min(1, to_point.dot(edge) / (edge_len * edge_len)))
                closest = v0 + edge * t

                dist = pos.distance_to(closest)
                if dist < min_dist:
                    min_dist = dist
                    nearest = closest

        return nearest

    def _update_ghost_face(self):
        """Update the ghost preview face"""
        if self.edit_mode != EditMode.PLACE or self.selected_tileset_index < 0:
            self.ghost_face = None
            return

        # Create ghost face at cursor position
        pos = self.cursor_world_pos
        size = self.grid_size
        half = size / 2.0

        # Create base vertices centered around the cursor, then offset
        # This allows proper rotation around center
        if self.plane_orientation == PlaneOrientation.FLOOR:
            # Horizontal face on XZ plane - centered for rotation
            base_verts = [
                Vec3(-half, 0, -half),
                Vec3(half, 0, -half),
                Vec3(half, 0, half),
                Vec3(-half, 0, half)
            ]
            # Apply face rotation around Y axis
            rotated = self._rotate_vertices_y(base_verts, self.face_rotation)
            # Offset to cursor position (corner-based placement)
            v0 = Vec3(pos.x + rotated[0].x + half, pos.y + rotated[0].y, pos.z + rotated[0].z + half)
            v1 = Vec3(pos.x + rotated[1].x + half, pos.y + rotated[1].y, pos.z + rotated[1].z + half)
            v2 = Vec3(pos.x + rotated[2].x + half, pos.y + rotated[2].y, pos.z + rotated[2].z + half)
            v3 = Vec3(pos.x + rotated[3].x + half, pos.y + rotated[3].y, pos.z + rotated[3].z + half)

        elif self.plane_orientation == PlaneOrientation.WALL_X:
            # Vertical face on YZ plane (wall facing X direction)
            base_verts = [
                Vec3(0, -half, -half),
                Vec3(0, -half, half),
                Vec3(0, half, half),
                Vec3(0, half, -half)
            ]
            # Apply face rotation around X axis (the normal of this wall)
            rotated = self._rotate_vertices_x(base_verts, self.face_rotation)
            # Offset to cursor position
            v0 = Vec3(pos.x, pos.y + rotated[0].y + half, pos.z + rotated[0].z + half)
            v1 = Vec3(pos.x, pos.y + rotated[1].y + half, pos.z + rotated[1].z + half)
            v2 = Vec3(pos.x, pos.y + rotated[2].y + half, pos.z + rotated[2].z + half)
            v3 = Vec3(pos.x, pos.y + rotated[3].y + half, pos.z + rotated[3].z + half)

        else:  # WALL_Z
            # Vertical face on XY plane (wall facing Z direction)
            base_verts = [
                Vec3(-half, -half, 0),
                Vec3(half, -half, 0),
                Vec3(half, half, 0),
                Vec3(-half, half, 0)
            ]
            # Apply face rotation around Z axis (the normal of this wall)
            rotated = self._rotate_vertices_z(base_verts, self.face_rotation)
            # Offset to cursor position
            v0 = Vec3(pos.x + rotated[0].x + half, pos.y + rotated[0].y + half, pos.z)
            v1 = Vec3(pos.x + rotated[1].x + half, pos.y + rotated[1].y + half, pos.z)
            v2 = Vec3(pos.x + rotated[2].x + half, pos.y + rotated[2].y + half, pos.z)
            v3 = Vec3(pos.x + rotated[3].x + half, pos.y + rotated[3].y + half, pos.z)

        self.ghost_face = TileFace(
            vertices=[v0, v1, v2, v3],
            tileset_index=self.selected_tileset_index,
            tile_index=self.selected_tile_index
        )

    def _rotate_vertices_y(self, vertices: List[Vec3], degrees: int) -> List[Vec3]:
        """Rotate vertices around Y axis"""
        rad = math.radians(degrees)
        cos_a = math.cos(rad)
        sin_a = math.sin(rad)
        result = []
        for v in vertices:
            new_x = v.x * cos_a - v.z * sin_a
            new_z = v.x * sin_a + v.z * cos_a
            result.append(Vec3(new_x, v.y, new_z))
        return result

    def _rotate_vertices_x(self, vertices: List[Vec3], degrees: int) -> List[Vec3]:
        """Rotate vertices around X axis"""
        rad = math.radians(degrees)
        cos_a = math.cos(rad)
        sin_a = math.sin(rad)
        result = []
        for v in vertices:
            new_y = v.y * cos_a - v.z * sin_a
            new_z = v.y * sin_a + v.z * cos_a
            result.append(Vec3(v.x, new_y, new_z))
        return result

    def _rotate_vertices_z(self, vertices: List[Vec3], degrees: int) -> List[Vec3]:
        """Rotate vertices around Z axis"""
        rad = math.radians(degrees)
        cos_a = math.cos(rad)
        sin_a = math.sin(rad)
        result = []
        for v in vertices:
            new_x = v.x * cos_a - v.y * sin_a
            new_y = v.x * sin_a + v.y * cos_a
            result.append(Vec3(new_x, new_y, v.z))
        return result

    def _flip_tile_orientation(self):
        """Flip the tile orientation between floor and wall modes (middle-click)"""
        # Cycle through: FLOOR -> WALL_Z -> WALL_X -> FLOOR
        if self.plane_orientation == PlaneOrientation.FLOOR:
            self.plane_orientation = PlaneOrientation.WALL_Z
        elif self.plane_orientation == PlaneOrientation.WALL_Z:
            self.plane_orientation = PlaneOrientation.WALL_X
        else:
            self.plane_orientation = PlaneOrientation.FLOOR

        # Update the combo box in the UI if available
        if self.parent_tool and hasattr(self.parent_tool, 'plane_combo'):
            if self.plane_orientation == PlaneOrientation.FLOOR:
                self.parent_tool.plane_combo.setCurrentIndex(0)
            elif self.plane_orientation == PlaneOrientation.WALL_X:
                self.parent_tool.plane_combo.setCurrentIndex(1)
            else:
                self.parent_tool.plane_combo.setCurrentIndex(2)

        self._update_ghost_face()
        print(f"[TileMap25D] Flipped to: {self.plane_orientation.value}")

    def _save_undo_state(self):
        """Save current document state to undo stack"""
        if not self.document:
            return

        # Save state as dict
        state = self.document.to_dict()
        self.undo_stack.append(state)

        # Limit undo stack size
        if len(self.undo_stack) > self.max_undo_levels:
            self.undo_stack.pop(0)

        # Clear redo stack when a new action is performed
        self.redo_stack.clear()

        # Update UI buttons if available
        if self.parent_tool:
            self.parent_tool._update_undo_redo_buttons()

    def _undo(self):
        """Undo the last action"""
        if not self.undo_stack or not self.document:
            return

        # Save current state to redo stack
        current_state = self.document.to_dict()
        self.redo_stack.append(current_state)

        # Restore previous state
        previous_state = self.undo_stack.pop()
        self.document.from_dict(previous_state, load_tilesets=False)

        # Clear selection
        self.selected_face_ids.clear()
        self.selected_vertex_indices.clear()
        self.selected_edge_indices.clear()

        # Sync to C++ backend
        self._sync_document_to_cpp()
        self.selection_changed.emit()

        # Update UI buttons
        if self.parent_tool:
            self.parent_tool._update_undo_redo_buttons()

        print(f"[TileMap25D] Undo - stack size: {len(self.undo_stack)}")

    def _redo(self):
        """Redo the last undone action"""
        if not self.redo_stack or not self.document:
            return

        # Save current state to undo stack
        current_state = self.document.to_dict()
        self.undo_stack.append(current_state)

        # Restore redo state
        redo_state = self.redo_stack.pop()
        self.document.from_dict(redo_state, load_tilesets=False)

        # Clear selection
        self.selected_face_ids.clear()
        self.selected_vertex_indices.clear()
        self.selected_edge_indices.clear()

        # Sync to C++ backend
        self._sync_document_to_cpp()
        self.selection_changed.emit()

        # Update UI buttons
        if self.parent_tool:
            self.parent_tool._update_undo_redo_buttons()

        print(f"[TileMap25D] Redo - stack size: {len(self.redo_stack)}")

    def _place_face(self):
        """Place a new face at the ghost position"""
        if not self.ghost_face or not self.document:
            return

        # Save undo state before placing
        self._save_undo_state()

        # Create new face from ghost
        new_face = self.ghost_face.copy()

        # Set UVs based on tile selection
        if self.document.tilesets and new_face.tileset_index < len(self.document.tilesets):
            tileset = self.document.tilesets[new_face.tileset_index]
            u_min, v_min, u_max, v_max = tileset.get_tile_uv_rect(new_face.tile_index)
            new_face.uvs = [
                Vec2(u_min, v_max),
                Vec2(u_max, v_max),
                Vec2(u_max, v_min),
                Vec2(u_min, v_min)
            ]

        self.document.add_face(new_face)

        # Sync to C++ backend
        if self.editor_bridge and self.tilemap_renderer_id:
            try:
                v = new_face.vertices
                uv = new_face.uvs
                self.editor_bridge.tilemap25d_add_face(
                    self.tilemap_renderer_id,
                    v[0].x, v[0].y, v[0].z,
                    v[1].x, v[1].y, v[1].z,
                    v[2].x, v[2].y, v[2].z,
                    v[3].x, v[3].y, v[3].z,
                    uv[0].u, uv[0].v,
                    uv[1].u, uv[1].v,
                    uv[2].u, uv[2].v,
                    uv[3].u, uv[3].v,
                    new_face.tileset_index, new_face.tile_index,
                    new_face.two_sided
                )
            except Exception as e:
                print(f"[TileMap25DViewport] Error adding face to C++: {e}")

        self.face_placed.emit()

    def _select_face_at_cursor(self):
        """Select face at cursor position"""
        if not self.document:
            return

        # Find face closest to cursor
        closest_face = None
        min_dist = float('inf')

        for face in self.document.faces:
            center = face.get_center()
            dist = self.cursor_world_pos.distance_to(center)
            if dist < min_dist and dist < self.grid_size * 2:
                min_dist = dist
                closest_face = face

        if closest_face:
            if closest_face.id in self.selected_face_ids:
                self.selected_face_ids.remove(closest_face.id)
            else:
                self.selected_face_ids.add(closest_face.id)
            self.selection_changed.emit()

    def _select_vertex_at_cursor(self):
        """Select vertex at cursor position"""
        if not self.document:
            return

        closest = None
        min_dist = float('inf')

        # Increased threshold from 0.5 to 0.75 for easier vertex selection
        selection_threshold = self.grid_size * 0.75

        for face in self.document.faces:
            for i, v in enumerate(face.vertices):
                dist = self.cursor_world_pos.distance_to(v)
                if dist < min_dist and dist < selection_threshold:
                    min_dist = dist
                    closest = (face.id, i)

        if closest:
            if closest in self.selected_vertex_indices:
                self.selected_vertex_indices.remove(closest)
            else:
                self.selected_vertex_indices.append(closest)
            self.selection_changed.emit()

    def _select_edge_at_cursor(self):
        """Select edge at cursor position"""
        if not self.document:
            return

        closest = None
        min_dist = float('inf')

        # Increased threshold from 0.3 to 0.5 for easier edge selection
        selection_threshold = self.grid_size * 0.5

        for face in self.document.faces:
            # Check distance to each edge (4 edges per face)
            for i in range(4):
                v1 = face.vertices[i]
                v2 = face.vertices[(i + 1) % 4]

                # Calculate distance from cursor to edge (line segment)
                dist = self._point_to_line_distance(self.cursor_world_pos, v1, v2)
                if dist < min_dist and dist < selection_threshold:
                    min_dist = dist
                    closest = (face.id, i)

        if closest:
            if closest in self.selected_edge_indices:
                self.selected_edge_indices.remove(closest)
            else:
                self.selected_edge_indices.append(closest)
            self.selection_changed.emit()

    def _point_to_line_distance(self, point: Vec3, line_start: Vec3, line_end: Vec3) -> float:
        """Calculate the distance from a point to a line segment"""
        line_vec = line_end - line_start
        point_vec = point - line_start

        line_len_sq = line_vec.x**2 + line_vec.y**2 + line_vec.z**2
        if line_len_sq < 0.0001:
            return point_vec.length()

        # Project point onto line
        t = max(0, min(1, point_vec.dot(line_vec) / line_len_sq))
        projection = line_start + line_vec * t

        return (point - projection).length()

    def _delete_face_at_cursor(self):
        """Delete face at cursor position"""
        if not self.document:
            return

        # Find face closest to cursor
        closest_face = None
        min_dist = float('inf')

        for face in self.document.faces:
            center = face.get_center()
            dist = self.cursor_world_pos.distance_to(center)
            if dist < min_dist and dist < self.grid_size * 2:
                min_dist = dist
                closest_face = face

        if closest_face:
            self._save_undo_state()
            self.document.remove_face(closest_face.id)
            self.selected_face_ids.discard(closest_face.id)

    def _paint_face_at_cursor(self):
        """Paint existing face with selected tile"""
        if not self.document or self.selected_tileset_index < 0:
            return

        # Find face closest to cursor
        closest_face = None
        min_dist = float('inf')

        for face in self.document.faces:
            center = face.get_center()
            dist = self.cursor_world_pos.distance_to(center)
            if dist < min_dist and dist < self.grid_size * 2:
                min_dist = dist
                closest_face = face

        if closest_face:
            self._save_undo_state()
            closest_face.tileset_index = self.selected_tileset_index
            closest_face.tile_index = self.selected_tile_index

            # Update UVs
            if self.document.tilesets and closest_face.tileset_index < len(self.document.tilesets):
                tileset = self.document.tilesets[closest_face.tileset_index]
                u_min, v_min, u_max, v_max = tileset.get_tile_uv_rect(closest_face.tile_index)
                closest_face.uvs = [
                    Vec2(u_min, v_max),
                    Vec2(u_max, v_max),
                    Vec2(u_max, v_min),
                    Vec2(u_min, v_min)
                ]

    def _drag_selected_vertices(self):
        """Drag selected vertices with mouse"""
        if not self.document:
            return

        delta = self.cursor_world_pos - self.drag_start_pos

        for face_id, vertex_idx in self.selected_vertex_indices:
            face = self.document.get_face_by_id(face_id)
            if face and 0 <= vertex_idx < len(face.vertices):
                new_pos = face.vertices[vertex_idx] + delta
                face.vertices[vertex_idx] = new_pos

                # Sync vertex position to C++ backend
                if self.editor_bridge and self.tilemap_renderer_id:
                    try:
                        self.editor_bridge.tilemap25d_set_face_vertex(
                            self.tilemap_renderer_id,
                            face_id,
                            vertex_idx,
                            new_pos.x, new_pos.y, new_pos.z
                        )
                    except Exception as e:
                        print(f"[TileMap25DViewport] Error updating vertex: {e}")

        self.drag_start_pos = self.cursor_world_pos.copy()

    def _drag_selected_edges(self):
        """Drag selected edges with mouse - moves both vertices of each edge"""
        if not self.document:
            return

        delta = self.cursor_world_pos - self.drag_start_pos

        for face_id, edge_idx in self.selected_edge_indices:
            face = self.document.get_face_by_id(face_id)
            if face:
                # Edge edge_idx connects vertex edge_idx to vertex (edge_idx+1)%4
                v1_idx = edge_idx
                v2_idx = (edge_idx + 1) % 4

                # Move both vertices
                new_pos1 = face.vertices[v1_idx] + delta
                new_pos2 = face.vertices[v2_idx] + delta
                face.vertices[v1_idx] = new_pos1
                face.vertices[v2_idx] = new_pos2

                # Sync vertex positions to C++ backend
                if self.editor_bridge and self.tilemap_renderer_id:
                    try:
                        self.editor_bridge.tilemap25d_set_face_vertex(
                            self.tilemap_renderer_id,
                            face_id,
                            v1_idx,
                            new_pos1.x, new_pos1.y, new_pos1.z
                        )
                        self.editor_bridge.tilemap25d_set_face_vertex(
                            self.tilemap_renderer_id,
                            face_id,
                            v2_idx,
                            new_pos2.x, new_pos2.y, new_pos2.z
                        )
                    except Exception as e:
                        print(f"[TileMap25DViewport] Error updating edge vertices: {e}")

        self.drag_start_pos = self.cursor_world_pos.copy()

    def _delete_selected(self):
        """Delete all selected faces"""
        if not self.document or not self.selected_face_ids:
            return

        self._save_undo_state()

        for face_id in list(self.selected_face_ids):
            self.document.remove_face(face_id)
            # Sync deletion to C++ backend
            if self.editor_bridge and self.tilemap_renderer_id:
                try:
                    self.editor_bridge.tilemap25d_remove_face(self.tilemap_renderer_id, face_id)
                except Exception as e:
                    print(f"[TileMap25DViewport] Error removing face from C++: {e}")

        self.selected_face_ids.clear()
        self.selected_vertex_indices.clear()
        self.selected_edge_indices.clear()
        self.selection_changed.emit()

    def _select_all(self):
        """Select all faces"""
        if not self.document:
            return

        self.selected_face_ids = {face.id for face in self.document.faces}
        self.selection_changed.emit()

    def cleanup(self):
        """Cleanup resources"""
        if self.render_timer:
            self.render_timer.stop()
        if self.editor_bridge:
            if self.view_id:
                try:
                    self.editor_bridge.destroy_render_view(self.view_id)
                except:
                    pass
            if self.tilemap_renderer_id:
                try:
                    self.editor_bridge.destroy_tilemap25d_builder(self.tilemap_renderer_id)
                except:
                    pass


class TileMap25DEditor(EditorPanel):
    """Main TileMap 2.5D Editor Tool"""

    def __init__(self, editor_bridge=None, parent=None, editor_session=None, project_root=None):
        self.editor_bridge = editor_bridge
        self.editor_session = editor_session
        self.project_root = project_root
        self.viewport = None
        self.document = TileMap25DDocument()
        self.current_file = None

        super().__init__("TileMap 2.5D Editor", parent)
        self.setMinimumSize(1000, 700)
        self.setObjectName("TileMap25DEditor")

    def showEvent(self, event):
        super().showEvent(event)
        if self.viewport and self.editor_bridge and self.editor_session and not self.viewport.view_id:
            QTimer.singleShot(100, self._init_viewport)

    def _init_viewport(self):
        if self.viewport and self.editor_bridge and self.editor_session and not self.viewport.view_id:
            self.viewport.initialize_rendering(self.editor_bridge, self.editor_session)

    def _setup_panel(self):
        """Setup the editor UI"""
        theme = get_theme_manager().get_current_theme()

        main_layout = QVBoxLayout()
        main_layout.setContentsMargins(4, 4, 4, 4)
        main_layout.setSpacing(4)

        # Toolbar
        toolbar = self._create_toolbar()
        main_layout.addLayout(toolbar)

        # Main splitter
        main_splitter = QSplitter(Qt.Orientation.Horizontal)

        # Left panel - Tilesets and palette
        left_panel = self._create_left_panel()
        main_splitter.addWidget(left_panel)

        # Center - Viewport
        self.viewport = TileMap25DViewport()
        self.viewport.parent_tool = self
        self.viewport.set_document(self.document)
        self.viewport.face_placed.connect(self._on_face_placed)
        self.viewport.selection_changed.connect(self._on_selection_changed)
        main_splitter.addWidget(self.viewport)

        # Right panel - Tools and properties
        right_panel = self._create_right_panel()
        main_splitter.addWidget(right_panel)

        main_splitter.setSizes([250, 600, 250])
        main_layout.addWidget(main_splitter, 1)

        # Status bar
        self.status_label = QLabel("Ready")
        main_layout.addWidget(self.status_label)

        container = QWidget()
        container.setLayout(main_layout)
        self.content_layout.addWidget(container)

        # Stats update timer
        self.stats_timer = QTimer(self)
        self.stats_timer.timeout.connect(self._update_stats)
        self.stats_timer.start(500)

    def _create_toolbar(self) -> QHBoxLayout:
        """Create the main toolbar"""
        toolbar = QHBoxLayout()

        # File operations
        new_btn = QPushButton("New")
        new_btn.clicked.connect(self._new_document)
        toolbar.addWidget(new_btn)

        load_btn = QPushButton("Load")
        load_btn.clicked.connect(self._load_document)
        toolbar.addWidget(load_btn)

        save_btn = QPushButton("Save")
        save_btn.clicked.connect(self._save_document)
        toolbar.addWidget(save_btn)

        save_as_btn = QPushButton("Save As...")
        save_as_btn.clicked.connect(self._save_document_as)
        toolbar.addWidget(save_as_btn)

        toolbar.addWidget(QLabel(" | "))

        # Undo/Redo buttons
        self.undo_btn = QPushButton("Undo")
        self.undo_btn.setToolTip("Undo (Ctrl+Z)")
        self.undo_btn.setEnabled(False)
        self.undo_btn.clicked.connect(self._do_undo)
        toolbar.addWidget(self.undo_btn)

        self.redo_btn = QPushButton("Redo")
        self.redo_btn.setToolTip("Redo (Ctrl+Y / Ctrl+Shift+Z)")
        self.redo_btn.setEnabled(False)
        self.redo_btn.clicked.connect(self._do_redo)
        toolbar.addWidget(self.redo_btn)

        toolbar.addWidget(QLabel(" | "))

        # Export
        export_menu_btn = QPushButton("Export")
        export_menu = QMenu()
        export_obj_action = export_menu.addAction("Export to OBJ...")
        export_obj_action.triggered.connect(self._export_obj)
        export_menu_btn.setMenu(export_menu)
        toolbar.addWidget(export_menu_btn)

        toolbar.addStretch()

        return toolbar

    def _create_left_panel(self) -> QWidget:
        """Create left panel with tilesets"""
        panel = QWidget()
        layout = QVBoxLayout()
        layout.setContentsMargins(4, 4, 4, 4)

        # Tileset management
        tileset_group = QGroupBox("Tilesets")
        tileset_layout = QVBoxLayout()

        btn_layout = QHBoxLayout()
        load_tileset_btn = QPushButton("Load Tileset")
        load_tileset_btn.clicked.connect(self._load_tileset)
        btn_layout.addWidget(load_tileset_btn)

        clear_tilesets_btn = QPushButton("Clear All")
        clear_tilesets_btn.clicked.connect(self._clear_tilesets)
        btn_layout.addWidget(clear_tilesets_btn)

        tileset_layout.addLayout(btn_layout)

        self.tileset_list = QListWidget()
        self.tileset_list.setMaximumHeight(100)
        tileset_layout.addWidget(self.tileset_list)

        tileset_group.setLayout(tileset_layout)
        layout.addWidget(tileset_group)

        # Tile palette
        palette_group = QGroupBox("Tile Palette")
        palette_layout = QVBoxLayout()

        palette_scroll = QScrollArea()
        palette_scroll.setWidgetResizable(True)

        self.palette_widget = TilePalette25DWidget()
        self.palette_widget.tile_selected.connect(self._on_tile_selected)
        palette_scroll.setWidget(self.palette_widget)

        palette_layout.addWidget(palette_scroll)
        palette_group.setLayout(palette_layout)
        layout.addWidget(palette_group, 1)

        panel.setLayout(layout)
        return panel

    def _create_right_panel(self) -> QWidget:
        """Create right panel with tools"""
        panel = QWidget()
        panel.setMinimumWidth(250)
        layout = QVBoxLayout()
        layout.setContentsMargins(4, 4, 4, 4)

        # Create tabs
        self.tabs = QTabWidget()

        # Edit tab
        edit_tab = self._create_edit_tab()
        self.tabs.addTab(edit_tab, "Edit")

        # Properties tab
        props_tab = self._create_properties_tab()
        self.tabs.addTab(props_tab, "Properties")

        layout.addWidget(self.tabs)

        panel.setLayout(layout)
        return panel

    def _create_edit_tab(self) -> QWidget:
        """Create edit tools tab"""
        tab = QWidget()
        layout = QVBoxLayout()

        # Edit mode
        mode_group = QGroupBox("Edit Mode")
        mode_layout = QVBoxLayout()

        self.mode_buttons = QButtonGroup()
        modes = [
            ("Place Face", EditMode.PLACE),
            ("Select Face", EditMode.SELECT_FACE),
            ("Select Vertex", EditMode.SELECT_VERTEX),
            ("Select Edge", EditMode.SELECT_EDGE),
            ("Delete", EditMode.DELETE),
            ("Paint", EditMode.PAINT),
        ]

        for i, (label, mode) in enumerate(modes):
            btn = QRadioButton(label)
            if i == 0:
                btn.setChecked(True)
            btn.toggled.connect(lambda checked, m=mode: self._set_edit_mode(m) if checked else None)
            self.mode_buttons.addButton(btn)
            mode_layout.addWidget(btn)

        mode_group.setLayout(mode_layout)
        layout.addWidget(mode_group)

        # Snap mode
        snap_group = QGroupBox("Snapping")
        snap_layout = QVBoxLayout()

        self.snap_combo = QComboBox()
        self.snap_combo.addItem("Grid Snap", SnapMode.GRID)
        self.snap_combo.addItem("Vertex Snap", SnapMode.VERTEX)
        self.snap_combo.addItem("Edge Snap", SnapMode.EDGE)
        self.snap_combo.addItem("No Snap", SnapMode.NONE)
        self.snap_combo.currentIndexChanged.connect(self._set_snap_mode)
        snap_layout.addWidget(self.snap_combo)

        snap_group.setLayout(snap_layout)
        layout.addWidget(snap_group)

        # Plane settings
        plane_group = QGroupBox("Working Plane")
        plane_layout = QFormLayout()

        self.plane_combo = QComboBox()
        self.plane_combo.addItem("Floor (XZ)", PlaneOrientation.FLOOR)
        self.plane_combo.addItem("Wall X (YZ)", PlaneOrientation.WALL_X)
        self.plane_combo.addItem("Wall Z (XY)", PlaneOrientation.WALL_Z)
        self.plane_combo.currentIndexChanged.connect(self._set_plane_orientation)
        plane_layout.addRow("Orientation:", self.plane_combo)

        self.floor_spin = QDoubleSpinBox()
        self.floor_spin.setRange(-1000, 1000)
        self.floor_spin.setValue(0)
        self.floor_spin.setSingleStep(1.0)
        self.floor_spin.valueChanged.connect(self._set_floor_height)
        plane_layout.addRow("Height (Q/E):", self.floor_spin)

        self.grid_spin = QDoubleSpinBox()
        self.grid_spin.setRange(0.1, 10)
        self.grid_spin.setValue(1.0)
        self.grid_spin.setSingleStep(0.1)
        self.grid_spin.valueChanged.connect(self._set_grid_size)
        plane_layout.addRow("Grid Size:", self.grid_spin)

        rotation_label = QLabel("Tab: Rotate face\nR: Flip orientation\nMiddle-click: Flip")
        plane_layout.addRow("Controls:", rotation_label)

        plane_group.setLayout(plane_layout)
        layout.addWidget(plane_group)

        # Display options
        display_group = QGroupBox("Display")
        display_layout = QVBoxLayout()

        self.ghost_check = QCheckBox("Show Ghost Preview")
        self.ghost_check.setChecked(True)
        self.ghost_check.toggled.connect(lambda c: setattr(self.viewport, 'show_ghost', c))
        display_layout.addWidget(self.ghost_check)

        self.grid_check = QCheckBox("Show Grid")
        self.grid_check.setChecked(True)
        self.grid_check.toggled.connect(lambda c: setattr(self.viewport, 'show_grid', c))
        display_layout.addWidget(self.grid_check)

        display_group.setLayout(display_layout)
        layout.addWidget(display_group)

        # Stats
        stats_group = QGroupBox("Statistics")
        stats_layout = QVBoxLayout()

        self.face_count_label = QLabel("Faces: 0")
        self.selected_label = QLabel("Selected: 0")
        stats_layout.addWidget(self.face_count_label)
        stats_layout.addWidget(self.selected_label)

        stats_group.setLayout(stats_layout)
        layout.addWidget(stats_group)

        layout.addStretch()
        tab.setLayout(layout)
        return tab

    def _create_properties_tab(self) -> QWidget:
        """Create properties tab"""
        tab = QWidget()
        layout = QVBoxLayout()

        # Face properties (when selected)
        face_group = QGroupBox("Selected Face")
        face_layout = QFormLayout()

        self.two_sided_check = QCheckBox()
        self.two_sided_check.toggled.connect(self._toggle_two_sided)
        face_layout.addRow("Two-Sided:", self.two_sided_check)

        self.visible_check = QCheckBox()
        self.visible_check.setChecked(True)
        self.visible_check.toggled.connect(self._toggle_visible)
        face_layout.addRow("Visible:", self.visible_check)

        face_group.setLayout(face_layout)
        layout.addWidget(face_group)

        # Export options
        export_group = QGroupBox("Export Options")
        export_layout = QVBoxLayout()

        self.merge_verts_check = QCheckBox("Merge Duplicate Vertices")
        self.merge_verts_check.setChecked(True)
        export_layout.addWidget(self.merge_verts_check)

        self.texture_mode_group = QButtonGroup()

        self.atlas_radio = QRadioButton("Texture Atlas")
        self.atlas_radio.setChecked(True)
        self.atlas_radio.setToolTip("Pack all tiles into a single atlas texture")
        self.texture_mode_group.addButton(self.atlas_radio, 0)
        export_layout.addWidget(self.atlas_radio)

        self.unwrap_radio = QRadioButton("Flat Unwrap")
        self.unwrap_radio.setToolTip("Bake textures as they appear in the map")
        self.texture_mode_group.addButton(self.unwrap_radio, 1)
        export_layout.addWidget(self.unwrap_radio)

        export_group.setLayout(export_layout)
        layout.addWidget(export_group)

        layout.addStretch()
        tab.setLayout(layout)
        return tab

    # --- Event handlers ---

    def _on_tile_selected(self, tileset_index: int, tile_index: int):
        """Handle tile selection from palette"""
        if self.viewport:
            self.viewport.selected_tileset_index = tileset_index
            self.viewport.selected_tile_index = tile_index

    def _do_undo(self):
        """Trigger undo from toolbar button"""
        if self.viewport:
            self.viewport._undo()

    def _do_redo(self):
        """Trigger redo from toolbar button"""
        if self.viewport:
            self.viewport._redo()

    def _update_undo_redo_buttons(self):
        """Update undo/redo button enabled states"""
        if self.viewport:
            self.undo_btn.setEnabled(len(self.viewport.undo_stack) > 0)
            self.redo_btn.setEnabled(len(self.viewport.redo_stack) > 0)

    def _on_face_placed(self):
        """Handle face placement"""
        self._update_stats()
        self._update_undo_redo_buttons()

    def _on_selection_changed(self):
        """Handle selection change"""
        self._update_stats()
        self._update_undo_redo_buttons()

        # Update property widgets based on selection
        if self.viewport and self.viewport.selected_face_ids:
            face_id = list(self.viewport.selected_face_ids)[0]
            face = self.document.get_face_by_id(face_id)
            if face:
                self.two_sided_check.blockSignals(True)
                self.visible_check.blockSignals(True)
                self.two_sided_check.setChecked(face.two_sided)
                self.visible_check.setChecked(face.visible)
                self.two_sided_check.blockSignals(False)
                self.visible_check.blockSignals(False)

    def _set_edit_mode(self, mode: EditMode):
        """Set edit mode"""
        if self.viewport:
            self.viewport.edit_mode = mode

    def _set_snap_mode(self, index: int):
        """Set snap mode"""
        if self.viewport:
            self.viewport.snap_mode = self.snap_combo.itemData(index)

    def _set_plane_orientation(self, index: int):
        """Set plane orientation"""
        if self.viewport:
            self.viewport.plane_orientation = self.plane_combo.itemData(index)

    def _set_floor_height(self, value: float):
        """Set floor height"""
        if self.viewport:
            self.viewport.floor_height = value

    def _set_grid_size(self, value: float):
        """Set grid size"""
        if self.viewport:
            self.viewport.grid_size = value
            self.document.grid_size = value

    def _toggle_two_sided(self, checked: bool):
        """Toggle two-sided for selected faces"""
        if self.viewport:
            for face_id in self.viewport.selected_face_ids:
                face = self.document.get_face_by_id(face_id)
                if face:
                    face.two_sided = checked

    def _toggle_visible(self, checked: bool):
        """Toggle visibility for selected faces"""
        if self.viewport:
            for face_id in self.viewport.selected_face_ids:
                face = self.document.get_face_by_id(face_id)
                if face:
                    face.visible = checked

    def _update_stats(self):
        """Update statistics labels"""
        face_count = len(self.document.faces)
        selected_count = len(self.viewport.selected_face_ids) if self.viewport else 0

        self.face_count_label.setText(f"Faces: {face_count}")
        self.selected_label.setText(f"Selected: {selected_count}")

    # --- File operations ---

    def _load_tileset(self):
        """Load a tileset file"""
        from panels.inspector_panel import convert_to_res_path

        path, _ = QFileDialog.getOpenFileName(
            self,
            "Load Tileset",
            str(self._get_tilesets_directory()),
            "Tileset 2D (*.tileset2D)"
        )

        if path:
            # Convert to res:// path for storage
            res_path = convert_to_res_path(path)
            tileset = TilesetData25D(file_path=res_path)
            if tileset.load():
                self.document.tilesets.append(tileset)
                self.palette_widget.add_tileset(tileset)
                self.tileset_list.addItem(tileset.name)

                # Send texture to C++ backend
                if self.viewport and self.viewport.editor_bridge and self.viewport.tilemap_renderer_id:
                    try:
                        if tileset.image_path:
                            self.viewport.editor_bridge.tilemap25d_set_texture(
                                self.viewport.tilemap_renderer_id,
                                tileset.image_path
                            )
                            print(f"[TileMap25D] Loaded texture to GPU: {tileset.image_path}")
                    except Exception as e:
                        print(f"[TileMap25D] Failed to load texture to GPU: {e}")

                QMessageBox.information(self, "Success", f"Loaded tileset: {tileset.name}")
            else:
                QMessageBox.warning(self, "Error", "Failed to load tileset")

    def _clear_tilesets(self):
        """Clear all tilesets"""
        self.document.tilesets.clear()
        self.palette_widget.clear_tilesets()
        self.tileset_list.clear()

    def _new_document(self):
        """Create new document"""
        reply = QMessageBox.question(
            self, "New Document",
            "Create new document? Unsaved changes will be lost.",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
        )

        if reply == QMessageBox.StandardButton.Yes:
            self.document = TileMap25DDocument()
            if self.viewport:
                self.viewport.set_document(self.document)
                self.viewport.selected_face_ids.clear()
                self.viewport.selected_vertex_indices.clear()
                self.viewport.selected_edge_indices.clear()
            self.current_file = None
            self.status_label.setText("New document created")

    def _load_document(self):
        """Load document from file"""
        path, _ = QFileDialog.getOpenFileName(
            self,
            "Load TileMap 2.5D",
            str(self._get_tilemaps_directory()),
            "TileMap 2.5D (*.tilemap25d)"
        )

        if path:
            try:
                path = path.replace("/", "\\")
                with open(path, 'r', encoding='utf-8') as f:
                    data = json.load(f)

                self.document = TileMap25DDocument()
                self.document.from_dict(data)

                # Update palette with loaded tilesets
                self.palette_widget.clear_tilesets()
                self.tileset_list.clear()
                for ts in self.document.tilesets:
                    self.palette_widget.add_tileset(ts)
                    self.tileset_list.addItem(ts.name)

                if self.viewport:
                    self.viewport.set_document(self.document)
                    self.viewport.selected_face_ids.clear()
                    self.viewport.selected_vertex_indices.clear()
                    self.viewport.selected_edge_indices.clear()

                self.current_file = path
                self.status_label.setText(f"Loaded: {path}")
                QMessageBox.information(self, "Success", "Document loaded successfully")
            except Exception as e:
                QMessageBox.warning(self, "Error", f"Failed to load document:\\n{str(e)}")

    def _save_document(self):
        """Save current document"""
        if not self.current_file:
            self._save_document_as()
            return

        self._do_save(self.current_file)

    def _save_document_as(self):
        """Save document with new name"""
        path, _ = QFileDialog.getSaveFileName(
            self,
            "Save TileMap 2.5D",
            str(self._get_tilemaps_directory()),
            "TileMap 2.5D (*.tilemap25d)"
        )

        if path:
            path = path.replace("/", "\\")
            if not path.endswith('.tilemap25d'):
                path += '.tilemap25d'

            self.current_file = path
            self._do_save(path)

    def _do_save(self, path: str):
        """Perform save operation"""
        try:
            data = self.document.to_dict()
            with open(path, 'w', encoding='utf-8') as f:
                json.dump(data, f, indent=2)

            self.status_label.setText(f"Saved: {path}")
            QMessageBox.information(self, "Success", "Document saved successfully")
        except Exception as e:
            QMessageBox.warning(self, "Error", f"Failed to save document:\\n{str(e)}")

    def _export_obj(self):
        """Export to OBJ file"""
        path, _ = QFileDialog.getSaveFileName(
            self,
            "Export to OBJ",
            str(self.project_root) if self.project_root else "",
            "OBJ Files (*.obj)"
        )

        if path:
            try:
                path = path.replace("/", "\\")
                if not path.endswith('.obj'):
                    path += '.obj'

                use_atlas = self.atlas_radio.isChecked()
                merge_verts = self.merge_verts_check.isChecked()

                # Use C++ backend for export if available
                if self.viewport and self.viewport.editor_bridge and self.viewport.tilemap_renderer_id:
                    # Export OBJ using C++ backend
                    obj_content = self.viewport.editor_bridge.tilemap25d_export_obj(
                        self.viewport.tilemap_renderer_id,
                        merge_verts,
                        use_atlas
                    )

                    # Export MTL file
                    mtl_content = self.viewport.editor_bridge.tilemap25d_export_mtl(
                        self.viewport.tilemap_renderer_id,
                        use_atlas
                    )

                    # Write OBJ file
                    with open(path, 'w', encoding='utf-8') as f:
                        f.write(obj_content)

                    # Write MTL file
                    mtl_path = path.replace('.obj', '.mtl')
                    with open(mtl_path, 'w', encoding='utf-8') as f:
                        f.write(mtl_content)

                    # Export texture atlas or copy tileset texture
                    texture_file = ""
                    if use_atlas:
                        # Use the exact filename that MTL expects
                        atlas_path = str(Path(path).parent / 'tilemap25d_atlas.png')
                        self._export_texture_atlas(atlas_path)
                        texture_file = "tilemap25d_atlas.png"
                        print(f"[TileMap25D Export] Created atlas texture: {atlas_path}")
                    else:
                        # Copy the first tileset texture to the export directory
                        if self.document.tilesets:
                            import shutil
                            tileset = self.document.tilesets[0]
                            if tileset.image_path and Path(tileset.image_path).exists():
                                dest_path = Path(path).parent / Path(tileset.image_path).name
                                shutil.copy(tileset.image_path, dest_path)
                                texture_file = Path(tileset.image_path).name
                                print(f"[TileMap25D Export] Copied tileset texture: {dest_path}")
                            else:
                                print(f"[TileMap25D Export] WARNING: Tileset image not found: {tileset.image_path}")
                        else:
                            print(f"[TileMap25D Export] WARNING: No tilesets available")

                    print(f"[TileMap25D Export] OBJ: {path}")
                    print(f"[TileMap25D Export] MTL: {mtl_path}")
                    print(f"[TileMap25D Export] Texture: {texture_file}")

                    QMessageBox.information(self, "Success", f"Exported to OBJ:\\n{path}\\n\\nFiles created:\\n- {Path(path).name}\\n- {Path(mtl_path).name}\\n- {texture_file}")
                else:
                    # Fallback to Python implementation
                    obj_content = self._generate_obj()
                    with open(path, 'w', encoding='utf-8') as f:
                        f.write(obj_content)

                    # Generate and write MTL file
                    mtl_content = self._generate_mtl(use_atlas)
                    mtl_path = path.replace('.obj', '.mtl')
                    with open(mtl_path, 'w', encoding='utf-8') as f:
                        f.write(mtl_content)

                    # Export texture atlas if that mode is selected
                    if use_atlas:
                        # Use the exact filename that MTL expects
                        atlas_path = str(Path(path).parent / 'tilemap25d_atlas.png')
                        self._export_texture_atlas(atlas_path)
                    else:
                        # Copy the first tileset texture
                        if self.document.tilesets:
                            import shutil
                            tileset = self.document.tilesets[0]
                            if tileset.image_path and Path(tileset.image_path).exists():
                                dest_path = Path(path).parent / Path(tileset.image_path).name
                                shutil.copy(tileset.image_path, dest_path)

                    QMessageBox.information(self, "Success", f"Exported to OBJ:\\n{path}")
            except Exception as e:
                import traceback
                traceback.print_exc()
                QMessageBox.warning(self, "Error", f"Failed to export OBJ:\\n{str(e)}")

    def _generate_obj(self) -> str:
        """Generate OBJ file content (fallback Python implementation)"""
        lines = []
        lines.append("# TileMap 2.5D Export")
        lines.append(f"# Faces: {len(self.document.faces)}")
        lines.append("")
        lines.append("mtllib tilemap25d.mtl")
        lines.append("usemtl tilemap_material")
        lines.append("")

        use_atlas = self.atlas_radio.isChecked()
        atlas_uv_map = {}

        if use_atlas:
            # Calculate atlas UVs
            total_tiles = sum(ts.get_tile_count() for ts in self.document.tilesets)
            atlas_size = int(math.ceil(math.sqrt(total_tiles)))

            current_tile = 0
            for ts_idx, ts in enumerate(self.document.tilesets):
                for tile_idx in range(ts.get_tile_count()):
                    atlas_row = current_tile // atlas_size
                    atlas_col = current_tile % atlas_size

                    u_min = atlas_col / atlas_size
                    v_min = atlas_row / atlas_size
                    u_max = (atlas_col + 1) / atlas_size
                    v_max = (atlas_row + 1) / atlas_size

                    atlas_uv_map[(ts_idx, tile_idx)] = (u_min, v_min, u_max, v_max)
                    current_tile += 1

        vertex_offset = 1
        normal_offset = 1

        for face in self.document.faces:
            if not face.visible:
                continue

            # Write vertices
            for v in face.vertices:
                lines.append(f"v {v.x} {v.y} {v.z}")

            # Write texture coordinates
            if use_atlas and (face.tileset_index, face.tile_index) in atlas_uv_map:
                # Use atlas UVs
                u_min, v_min, u_max, v_max = atlas_uv_map[(face.tileset_index, face.tile_index)]
                lines.append(f"vt {u_min} {1.0 - v_min}")  # Bottom-left
                lines.append(f"vt {u_max} {1.0 - v_min}")  # Bottom-right
                lines.append(f"vt {u_max} {1.0 - v_max}")  # Top-right
                lines.append(f"vt {u_min} {1.0 - v_max}")  # Top-left
            else:
                # Use per-face UVs
                for uv in face.uvs:
                    lines.append(f"vt {uv.u} {1.0 - uv.v}")  # Flip V for OBJ

            # Write face indices
            v0 = vertex_offset
            v1 = vertex_offset + 1
            v2 = vertex_offset + 2
            v3 = vertex_offset + 3

            vt0 = vertex_offset
            vt1 = vertex_offset + 1
            vt2 = vertex_offset + 2
            vt3 = vertex_offset + 3

            # Choose better diagonal for non-planar quads to avoid "poked" appearance
            # Calculate normals for both diagonal options
            v0_pos = face.vertices[0]
            v1_pos = face.vertices[1]
            v2_pos = face.vertices[2]
            v3_pos = face.vertices[3]

            # Diagonal 0-2: triangles (0,1,2) and (0,2,3)
            edge1_02_t1 = Vec3(v1_pos.x - v0_pos.x, v1_pos.y - v0_pos.y, v1_pos.z - v0_pos.z)
            edge2_02_t1 = Vec3(v2_pos.x - v0_pos.x, v2_pos.y - v0_pos.y, v2_pos.z - v0_pos.z)
            n1_diag02 = edge1_02_t1.cross(edge2_02_t1).normalized()

            edge1_02_t2 = Vec3(v2_pos.x - v0_pos.x, v2_pos.y - v0_pos.y, v2_pos.z - v0_pos.z)
            edge2_02_t2 = Vec3(v3_pos.x - v0_pos.x, v3_pos.y - v0_pos.y, v3_pos.z - v0_pos.z)
            n2_diag02 = edge1_02_t2.cross(edge2_02_t2).normalized()

            # Diagonal 1-3: triangles (0,1,3) and (1,2,3)
            edge1_13_t1 = Vec3(v1_pos.x - v0_pos.x, v1_pos.y - v0_pos.y, v1_pos.z - v0_pos.z)
            edge2_13_t1 = Vec3(v3_pos.x - v0_pos.x, v3_pos.y - v0_pos.y, v3_pos.z - v0_pos.z)
            n1_diag13 = edge1_13_t1.cross(edge2_13_t1).normalized()

            edge1_13_t2 = Vec3(v2_pos.x - v1_pos.x, v2_pos.y - v1_pos.y, v2_pos.z - v1_pos.z)
            edge2_13_t2 = Vec3(v3_pos.x - v1_pos.x, v3_pos.y - v1_pos.y, v3_pos.z - v1_pos.z)
            n2_diag13 = edge1_13_t2.cross(edge2_13_t2).normalized()

            # Calculate dot products (closer to 1.0 means more coplanar)
            dot_diag02 = n1_diag02.dot(n2_diag02)
            dot_diag13 = n1_diag13.dot(n2_diag13)

            use_diag_02 = dot_diag02 >= dot_diag13

            if use_diag_02:
                # Write normals for this diagonal
                lines.append(f"vn {n1_diag02.x} {n1_diag02.y} {n1_diag02.z}")
                lines.append(f"vn {n2_diag02.x} {n2_diag02.y} {n2_diag02.z}")

                vn1 = normal_offset
                vn2 = normal_offset + 1

                # Two triangles for quad using 0-2 diagonal
                lines.append(f"f {v0}/{vt0}/{vn1} {v1}/{vt1}/{vn1} {v2}/{vt2}/{vn1}")
                lines.append(f"f {v0}/{vt0}/{vn2} {v2}/{vt2}/{vn2} {v3}/{vt3}/{vn2}")

                if face.two_sided:
                    # Back face normals (flipped)
                    n1_back = n1_diag02 * -1.0
                    n2_back = n2_diag02 * -1.0
                    lines.append(f"vn {n1_back.x} {n1_back.y} {n1_back.z}")
                    lines.append(f"vn {n2_back.x} {n2_back.y} {n2_back.z}")

                    vn1_back = normal_offset + 2
                    vn2_back = normal_offset + 3

                    # Back face (reverse winding)
                    lines.append(f"f {v2}/{vt2}/{vn1_back} {v1}/{vt1}/{vn1_back} {v0}/{vt0}/{vn1_back}")
                    lines.append(f"f {v3}/{vt3}/{vn2_back} {v2}/{vt2}/{vn2_back} {v0}/{vt0}/{vn2_back}")

                    normal_offset += 4
                else:
                    normal_offset += 2
            else:
                # Write normals for this diagonal
                lines.append(f"vn {n1_diag13.x} {n1_diag13.y} {n1_diag13.z}")
                lines.append(f"vn {n2_diag13.x} {n2_diag13.y} {n2_diag13.z}")

                vn1 = normal_offset
                vn2 = normal_offset + 1

                # Two triangles for quad using 1-3 diagonal
                lines.append(f"f {v0}/{vt0}/{vn1} {v1}/{vt1}/{vn1} {v3}/{vt3}/{vn1}")
                lines.append(f"f {v1}/{vt1}/{vn2} {v2}/{vt2}/{vn2} {v3}/{vt3}/{vn2}")

                if face.two_sided:
                    # Back face normals (flipped)
                    n1_back = n1_diag13 * -1.0
                    n2_back = n2_diag13 * -1.0
                    lines.append(f"vn {n1_back.x} {n1_back.y} {n1_back.z}")
                    lines.append(f"vn {n2_back.x} {n2_back.y} {n2_back.z}")

                    vn1_back = normal_offset + 2
                    vn2_back = normal_offset + 3

                    # Back face (reverse winding)
                    lines.append(f"f {v3}/{vt3}/{vn1_back} {v1}/{vt1}/{vn1_back} {v0}/{vt0}/{vn1_back}")
                    lines.append(f"f {v3}/{vt3}/{vn2_back} {v2}/{vt2}/{vn2_back} {v1}/{vt1}/{vn2_back}")

                    normal_offset += 4
                else:
                    normal_offset += 2

            vertex_offset += 4

        return "\n".join(lines)

    def _generate_mtl(self, use_atlas: bool) -> str:
        """Generate MTL file content"""
        lines = []
        lines.append("# TileMap 2.5D Material Export")
        lines.append(f"# Texture Mode: {'Atlas' if use_atlas else 'Per-Face UVs'}")
        lines.append("")
        lines.append("newmtl tilemap_material")
        lines.append("Ka 1.0 1.0 1.0")  # Ambient color
        lines.append("Kd 1.0 1.0 1.0")  # Diffuse color
        lines.append("Ks 0.0 0.0 0.0")  # Specular color
        lines.append("Ns 10.0")          # Specular exponent
        lines.append("d 1.0")            # Dissolve (opacity)
        lines.append("illum 1")          # Illumination model (1 = diffuse)

        if use_atlas:
            lines.append("map_Kd tilemap25d_atlas.png")
        else:
            # Reference the first tileset texture
            if self.document.tilesets:
                tileset = self.document.tilesets[0]
                if tileset.image_path:
                    texture_name = Path(tileset.image_path).name
                    lines.append(f"map_Kd {texture_name}")

        lines.append("")
        return "\n".join(lines)

    def _export_texture_atlas(self, path: str):
        """Export texture atlas"""
        if not self.document.tilesets:
            return

        # Calculate atlas size (simple packing)
        total_tiles = sum(ts.get_tile_count() for ts in self.document.tilesets)
        atlas_size = int(math.ceil(math.sqrt(total_tiles)))

        max_tile_w = max(ts.tile_width for ts in self.document.tilesets)
        max_tile_h = max(ts.tile_height for ts in self.document.tilesets)

        atlas_width = atlas_size * max_tile_w
        atlas_height = atlas_size * max_tile_h

        # Create atlas image
        atlas = QImage(atlas_width, atlas_height, QImage.Format.Format_ARGB32)
        atlas.fill(Qt.GlobalColor.transparent)

        painter = QPainter(atlas)

        tile_idx = 0
        for ts in self.document.tilesets:
            for i in range(ts.get_tile_count()):
                tile_pixmap = ts.get_tile_pixmap(i)

                row = tile_idx // atlas_size
                col = tile_idx % atlas_size

                x = col * max_tile_w
                y = row * max_tile_h

                painter.drawPixmap(x, y, tile_pixmap)
                tile_idx += 1

        painter.end()
        atlas.save(path)

    def _get_tilemaps_directory(self) -> Path:
        """Get tilemaps directory"""
        if self.project_root:
            dir_path = Path(self.project_root) / "assets" / "tilemaps"
            dir_path.mkdir(parents=True, exist_ok=True)
            return dir_path
        return Path.home()

    def _get_tilesets_directory(self) -> Path:
        """Get tilesets directory"""
        if self.project_root:
            dir_path = Path(self.project_root) / "assets" / "tilesets"
            dir_path.mkdir(parents=True, exist_ok=True)
            return dir_path
        return Path.home()

    def closeEvent(self, event):
        if hasattr(self, 'stats_timer') and self.stats_timer:
            self.stats_timer.stop()
        if self.viewport:
            self.viewport.cleanup()
        super().closeEvent(event)
