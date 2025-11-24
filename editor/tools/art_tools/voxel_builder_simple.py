"""
Voxel Builder Tool - Full Implementation
A complete voxel modeling tool with C++ backend
"""

from PyQt6.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QLabel,
                             QPushButton, QFileDialog, QMessageBox,
                             QSpinBox, QDoubleSpinBox, QSplitter, QGroupBox,
                             QRadioButton, QButtonGroup, QColorDialog,
                             QCheckBox, QSizePolicy, QTabWidget, QFormLayout, QComboBox)
from PyQt6.QtCore import Qt, QTimer, pyqtSignal, QPoint
from PyQt6.QtGui import QColor, QPalette, QMouseEvent, QWheelEvent, QKeyEvent
from pathlib import Path
import json
import sys
import math
from typing import List, Optional, Tuple, Dict, Set
from dataclasses import dataclass
from enum import Enum

# Import base panel
editor_path = Path(__file__).parent.parent.parent
if str(editor_path) not in sys.path:
    sys.path.insert(0, str(editor_path))

from panels.base_panel import EditorPanel
from theme import get_theme_manager

try:
    import lupine_engine as le
    HAS_ENGINE = True
except ImportError:
    HAS_ENGINE = False


class EditMode(Enum):
    PLACE = "place"
    ERASE = "erase"
    SELECT = "select"
    FILL = "fill"
    LINE = "line"
    RECT = "rect"
    SPHERE = "sphere"


class PlacementMode(Enum):
    SNAP_GRID = "snap_grid"
    SNAP_FACE = "snap_face"
    FREE = "free"


class WorkingSide(Enum):
    """Which side/plane the user is working on"""
    FLOOR = "floor"      # XZ plane, Y up (default, looking down)
    LEFT = "left"        # YZ plane, X up (looking from left)
    RIGHT = "right"      # YZ plane, -X up (looking from right)
    FORWARD = "forward"  # XY plane, Z up (looking from front)
    BACKWARD = "backward" # XY plane, -Z up (looking from back)


@dataclass
class Voxel:
    x: int
    y: int
    z: int
    color: Tuple[int, int, int, int] = (255, 255, 255, 255)  # RGBA


class VoxelViewport(QWidget):
    """Viewport that uses C++ VoxelBuilder for rendering"""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setMinimumSize(400, 300)
        self.setFocusPolicy(Qt.FocusPolicy.StrongFocus)

        # Set background color
        palette = self.palette()
        palette.setColor(QPalette.ColorRole.Window, QColor(30, 30, 30))
        self.setAutoFillBackground(True)
        self.setPalette(palette)

        # Render widget
        self.render_widget = QWidget(self)
        self.render_widget.setAutoFillBackground(True)
        self.render_widget.setMinimumSize(400, 300)
        render_palette = self.render_widget.palette()
        render_palette.setColor(QPalette.ColorRole.Window, QColor(30, 30, 30))
        self.render_widget.setPalette(render_palette)
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
        self.voxel_builder_id = None  # C++ VoxelBuilder instance ID
        self.ghost_voxel_builder_id = None  # For ghost preview

        # Render timer
        self.render_timer = None

        # Mouse state
        self.last_mouse_pos = QPoint()
        self.middle_mouse_down = False
        self.right_mouse_down = False
        self.left_mouse_down = False
        self.is_dragging = False

        # Edit state
        self.edit_mode = EditMode.PLACE
        self.placement_mode = PlacementMode.SNAP_GRID
        self.working_side = WorkingSide.FLOOR
        self.current_color = (255, 255, 255, 255)
        self.cursor_pos = [0, 0, 0]
        self.grid_floor_y = 0
        self.show_ghost = True
        self.show_grid = True
        self.voxel_rotation_y = 0  # Rotation in degrees (0, 15, 30, 45, etc.)

        # Shape tool state
        self.shape_start_pos = None

        # Selection state
        self.selected_voxels = set()  # Set of (x, y, z) tuples

        # Parent reference for updating UI
        self.parent_tool = None

    def initialize_rendering(self, editor_bridge, editor_session):
        """Initialize rendering using C++ VoxelBuilder"""
        if not HAS_ENGINE:
            return

        self.editor_bridge = editor_bridge

        # Create C++ VoxelBuilder instances (main and ghost)
        self.voxel_builder_id = self.editor_bridge.create_voxel_builder()
        self.ghost_voxel_builder_id = self.editor_bridge.create_voxel_builder()

        window_handle = int(self.render_widget.winId())
        dpr = self.render_widget.devicePixelRatio()
        width = int(self.render_widget.width() * dpr)
        height = int(self.render_widget.height() * dpr)

        # Create minimal empty scene (engine requirement for view)
        # Note: We don't need actual lighting nodes - the voxel renderer
        # provides its own simple lighting to make voxels easier to see
        scene_doc = editor_session.create_scene("Voxel Viewport")
        if scene_doc:
            scene = scene_doc.get_scene()

            # Create render view
            self.view_id = self.editor_bridge.create_render_view(window_handle, width, height)

            if self.view_id:
                self.editor_bridge.set_view_mode(self.view_id, le.ViewMode.View3D)
                self.editor_bridge.set_view_scene(self.view_id, scene)

                # Start render timer
                self.render_timer = QTimer(self)
                self.render_timer.timeout.connect(self.render_frame)
                self.render_timer.start(16)  # ~60 FPS

    def render_frame(self):
        """Render the current frame using C++ VoxelBuilder"""
        if not self.editor_bridge or not self.view_id or not self.voxel_builder_id:
            return

        try:
            # Update ghost voxel
            self._update_ghost_voxel()

            # Render both voxel builders to the view
            self.editor_bridge.voxel_builder_render(self.voxel_builder_id, self.view_id)
            if self.show_ghost and self.ghost_voxel_builder_id:
                self.editor_bridge.voxel_builder_render(self.ghost_voxel_builder_id, self.view_id)

            # Render selection bounding boxes
            self._render_selection_boxes()

            # Render floor grid
            self._render_floor_grid()

            # Render floor indicator when floor is not at 0
            self._render_floor_indicator()

            # Render the view itself
            self.editor_bridge.render_view(self.view_id)
        except Exception as e:
            print(f"[VoxelViewport] Error rendering: {e}")

    def _update_ghost_voxel(self):
        """Update the ghost voxel preview"""
        if not self.ghost_voxel_builder_id or not self.editor_bridge:
            return

        # Clear previous ghost
        self.editor_bridge.voxel_builder_clear(self.ghost_voxel_builder_id)

        if not self.show_ghost:
            return

        # Set ghost color based on edit mode
        # Blue for placing, Red for erasing, Orange for selecting
        x, y, z = self.cursor_pos
        a = 0.5  # Half opacity

        if self.edit_mode == EditMode.PLACE:
            # Bluish ghost (light blue/cyan)
            r, g, b = 0.3, 0.6, 1.0
            # Single voxel ghost
            self.editor_bridge.voxel_builder_place_voxel(
                self.ghost_voxel_builder_id, x, y, z, r, g, b, a
            )
        elif self.edit_mode == EditMode.ERASE:
            # Reddish ghost - renders in front of the voxel to be erased
            r, g, b = 1.0, 0.3, 0.3
            # Ghost at same position, will render on top due to draw order
            self.editor_bridge.voxel_builder_place_voxel(
                self.ghost_voxel_builder_id, x, y, z, r, g, b, a
            )
        elif self.edit_mode == EditMode.SELECT:
            # Orange ghost - renders in front of the voxel to be selected
            r, g, b = 1.0, 0.6, 0.2
            # Ghost at same position, will render on top due to draw order
            self.editor_bridge.voxel_builder_place_voxel(
                self.ghost_voxel_builder_id, x, y, z, r, g, b, a
            )
        elif self.edit_mode in [EditMode.LINE, EditMode.RECT, EditMode.SPHERE]:
            # Show shape preview if we have a start position
            # Use current color for these
            r = self.current_color[0] / 255.0
            g = self.current_color[1] / 255.0
            b = self.current_color[2] / 255.0
            if self.shape_start_pos:
                self._draw_shape_preview(self.shape_start_pos, [x, y, z], r, g, b, a)

    def _draw_shape_preview(self, start, end, r, g, b, a):
        """Draw preview for shape tools"""
        if self.edit_mode == EditMode.LINE:
            voxels = self._bresenham_line_3d(start, end)
            for vx, vy, vz in voxels:
                self.editor_bridge.voxel_builder_place_voxel(
                    self.ghost_voxel_builder_id, vx, vy, vz, r, g, b, a
                )

        elif self.edit_mode == EditMode.RECT:
            x1, y1, z1 = start
            x2, y2, z2 = end
            min_x, max_x = min(x1, x2), max(x1, x2)
            min_y, max_y = min(y1, y2), max(y1, y2)
            min_z, max_z = min(z1, z2), max(z1, z2)

            for x in range(min_x, max_x + 1):
                for y in range(min_y, max_y + 1):
                    for z in range(min_z, max_z + 1):
                        self.editor_bridge.voxel_builder_place_voxel(
                            self.ghost_voxel_builder_id, x, y, z, r, g, b, a
                        )

        elif self.edit_mode == EditMode.SPHERE:
            cx = (start[0] + end[0]) / 2.0
            cy = (start[1] + end[1]) / 2.0
            cz = (start[2] + end[2]) / 2.0
            radius = max(1, int(math.sqrt(
                (end[0] - start[0])**2 +
                (end[1] - start[1])**2 +
                (end[2] - start[2])**2
            ) / 2.0))

            for x in range(int(cx - radius), int(cx + radius) + 1):
                for y in range(int(cy - radius), int(cy + radius) + 1):
                    for z in range(int(cz - radius), int(cz + radius) + 1):
                        dist = math.sqrt((x - cx)**2 + (y - cy)**2 + (z - cz)**2)
                        if dist <= radius:
                            self.editor_bridge.voxel_builder_place_voxel(
                                self.ghost_voxel_builder_id, x, y, z, r, g, b, a
                            )

    def _bresenham_line_3d(self, start, end):
        """3D Bresenham line algorithm"""
        voxels = []
        x1, y1, z1 = start
        x2, y2, z2 = end

        dx = abs(x2 - x1)
        dy = abs(y2 - y1)
        dz = abs(z2 - z1)

        xs = 1 if x2 > x1 else -1
        ys = 1 if y2 > y1 else -1
        zs = 1 if z2 > z1 else -1

        # Driving axis is X
        if dx >= dy and dx >= dz:
            p1 = 2 * dy - dx
            p2 = 2 * dz - dx
            while x1 != x2:
                voxels.append((x1, y1, z1))
                x1 += xs
                if p1 >= 0:
                    y1 += ys
                    p1 -= 2 * dx
                if p2 >= 0:
                    z1 += zs
                    p2 -= 2 * dx
                p1 += 2 * dy
                p2 += 2 * dz
        # Driving axis is Y
        elif dy >= dx and dy >= dz:
            p1 = 2 * dx - dy
            p2 = 2 * dz - dy
            while y1 != y2:
                voxels.append((x1, y1, z1))
                y1 += ys
                if p1 >= 0:
                    x1 += xs
                    p1 -= 2 * dy
                if p2 >= 0:
                    z1 += zs
                    p2 -= 2 * dy
                p1 += 2 * dx
                p2 += 2 * dz
        # Driving axis is Z
        else:
            p1 = 2 * dy - dz
            p2 = 2 * dx - dz
            while z1 != z2:
                voxels.append((x1, y1, z1))
                z1 += zs
                if p1 >= 0:
                    y1 += ys
                    p1 -= 2 * dz
                if p2 >= 0:
                    x1 += xs
                    p2 -= 2 * dz
                p1 += 2 * dy
                p2 += 2 * dx

        voxels.append((x1, y1, z1))
        return voxels

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

        if event.button() == Qt.MouseButton.LeftButton:
            self.left_mouse_down = True
            # Handle voxel operations
            if self.edit_mode == EditMode.PLACE:
                self.place_voxel(self.cursor_pos[0], self.cursor_pos[1], self.cursor_pos[2])
            elif self.edit_mode == EditMode.ERASE:
                self.erase_voxel(self.cursor_pos[0], self.cursor_pos[1], self.cursor_pos[2])
            elif self.edit_mode == EditMode.SELECT:
                # Toggle selection at cursor position
                voxel_key = tuple(self.cursor_pos)
                if voxel_key in self.selected_voxels:
                    self.selected_voxels.remove(voxel_key)
                else:
                    self.selected_voxels.add(voxel_key)
            elif self.edit_mode in [EditMode.LINE, EditMode.RECT, EditMode.SPHERE]:
                # Start shape
                self.shape_start_pos = self.cursor_pos.copy()
                self.is_dragging = True
            elif self.edit_mode == EditMode.FILL:
                self._flood_fill(self.cursor_pos[0], self.cursor_pos[1], self.cursor_pos[2])
        elif event.button() == Qt.MouseButton.MiddleButton:
            self.middle_mouse_down = True
        elif event.button() == Qt.MouseButton.RightButton:
            self.right_mouse_down = True

    def _handle_mouse_release(self, event: QMouseEvent):
        if event.button() == Qt.MouseButton.LeftButton:
            self.left_mouse_down = False
            if self.is_dragging and self.shape_start_pos:
                # Complete shape
                self._complete_shape(self.shape_start_pos, self.cursor_pos)
                self.shape_start_pos = None
                self.is_dragging = False
        elif event.button() == Qt.MouseButton.MiddleButton:
            self.middle_mouse_down = False
        elif event.button() == Qt.MouseButton.RightButton:
            self.right_mouse_down = False

    def _handle_mouse_move(self, event: QMouseEvent):
        if not self.editor_bridge or not self.view_id:
            return

        current_pos = event.pos()
        delta_x = current_pos.x() - self.last_mouse_pos.x()
        delta_y = current_pos.y() - self.last_mouse_pos.y()

        # Store previous cursor position for continuous painting
        prev_cursor_pos = self.cursor_pos.copy()

        # Update cursor position based on placement mode
        self._update_cursor_position(current_pos)

        # Camera controls - USE EDITOR BRIDGE METHODS like main viewport
        if self.middle_mouse_down:
            # Pan camera
            self.editor_bridge.pan_camera_3d(self.view_id, float(delta_x), float(delta_y))
        elif self.right_mouse_down:
            # Orbit camera
            self.editor_bridge.orbit_camera_3d(self.view_id, float(delta_x), float(delta_y))
        elif self.left_mouse_down and not self.is_dragging:
            # Continuous painting when left mouse is down (not for shape tools)
            if self.edit_mode == EditMode.PLACE:
                # Only place if cursor moved to a new position
                if prev_cursor_pos != self.cursor_pos:
                    self.place_voxel(self.cursor_pos[0], self.cursor_pos[1], self.cursor_pos[2])
            elif self.edit_mode == EditMode.ERASE:
                # Only erase if cursor moved to a new position
                if prev_cursor_pos != self.cursor_pos:
                    self.erase_voxel(self.cursor_pos[0], self.cursor_pos[1], self.cursor_pos[2])

        self.last_mouse_pos = current_pos

    def _update_cursor_position(self, mouse_pos):
        """Update cursor position based on placement mode and working side"""
        if not self.editor_bridge or not self.view_id:
            return

        # Use proper screen-to-world conversion with ray casting
        dpr = self.render_widget.devicePixelRatio()
        screen_x = mouse_pos.x() * dpr
        screen_y = mouse_pos.y() * dpr

        try:
            # Determine the plane coordinate based on working side
            # For different sides, the "floor_y" represents different axes
            plane_coord = float(self.grid_floor_y)

            # Cast ray to intersect with the working plane
            # screen_to_world_3d returns a dict with 'x', 'y', 'z' keys
            world_pos = self.editor_bridge.screen_to_world_3d(
                self.view_id,
                float(screen_x),
                float(screen_y),
                plane_coord
            )

            # Validate the result
            if not world_pos or not isinstance(world_pos, dict):
                return

            if 'x' not in world_pos or 'y' not in world_pos or 'z' not in world_pos:
                return

            # Extract world coordinates from dict
            wx = world_pos['x']
            wy = world_pos['y']
            wz = world_pos['z']

            # Adjust based on working side
            # The working side determines which coordinate is the "floor" level
            if self.working_side == WorkingSide.FLOOR:
                # Default: XZ plane, Y is up
                wy = plane_coord
            elif self.working_side == WorkingSide.LEFT:
                # YZ plane, X is up/right
                wx = plane_coord
            elif self.working_side == WorkingSide.RIGHT:
                # YZ plane, -X is up/left
                wx = -plane_coord
            elif self.working_side == WorkingSide.FORWARD:
                # XY plane, Z is up/forward
                wz = plane_coord
            elif self.working_side == WorkingSide.BACKWARD:
                # XY plane, -Z is up/backward
                wz = -plane_coord

            # Apply placement mode
            if self.placement_mode == PlacementMode.SNAP_GRID:
                # Snap to nearest grid position
                self.cursor_pos = [int(round(wx)), int(round(wy)), int(round(wz))]
            elif self.placement_mode == PlacementMode.FREE:
                # Truncate to integer positions (floor behavior)
                self.cursor_pos = [int(wx), int(wy), int(wz)]
            else:  # SNAP_FACE
                # Snap to the nearest voxel face
                # First snap to grid
                grid_x = int(round(wx))
                grid_y = int(round(wy))
                grid_z = int(round(wz))

                # Check if there's a voxel at this position
                if self.voxel_builder_id and self.editor_bridge:
                    has_voxel = self.editor_bridge.voxel_builder_has_voxel(
                        self.voxel_builder_id, grid_x, grid_y, grid_z
                    )

                    if has_voxel:
                        # There's a voxel here, offset to adjacent empty space
                        # Determine which face is closest based on fractional part
                        fx = wx - grid_x
                        fy = wy - grid_y
                        fz = wz - grid_z

                        # Find which axis has the largest fractional deviation
                        abs_fx = abs(fx)
                        abs_fy = abs(fy)
                        abs_fz = abs(fz)

                        if abs_fx >= abs_fy and abs_fx >= abs_fz:
                            # X axis dominant
                            grid_x += 1 if fx > 0 else -1
                        elif abs_fy >= abs_fx and abs_fy >= abs_fz:
                            # Y axis dominant
                            grid_y += 1 if fy > 0 else -1
                        else:
                            # Z axis dominant
                            grid_z += 1 if fz > 0 else -1

                self.cursor_pos = [grid_x, grid_y, grid_z]

        except Exception as e:
            print(f"[VoxelViewport] Error updating cursor position: {e}")
            # Keep previous cursor position on error

    def _handle_wheel(self, event: QWheelEvent):
        if not self.editor_bridge or not self.view_id:
            return

        # Zoom camera - USE EDITOR BRIDGE METHOD like main viewport
        delta = event.angleDelta().y() / 120.0
        self.editor_bridge.zoom_camera_3d(self.view_id, delta)

    def _handle_key_press(self, event: QKeyEvent):
        """Handle keyboard shortcuts"""
        key = event.key()

        # Q/E - Lower/Raise floor height
        if key == Qt.Key.Key_Q:
            self.grid_floor_y -= 1
            if self.parent_tool and hasattr(self.parent_tool, 'floor_spin'):
                self.parent_tool.floor_spin.setValue(self.grid_floor_y)
        elif key == Qt.Key.Key_E:
            self.grid_floor_y += 1
            if self.parent_tool and hasattr(self.parent_tool, 'floor_spin'):
                self.parent_tool.floor_spin.setValue(self.grid_floor_y)

        # R - Rotate voxel by 15 degrees
        elif key == Qt.Key.Key_R:
            self.voxel_rotation_y = (self.voxel_rotation_y + 15) % 360
            print(f"[VoxelViewport] Voxel rotation: {self.voxel_rotation_y}°")

    def place_voxel(self, x, y, z):
        """Place a voxel using C++ backend"""
        if not self.voxel_builder_id or not self.editor_bridge:
            return
        r = self.current_color[0] / 255.0
        g = self.current_color[1] / 255.0
        b = self.current_color[2] / 255.0
        a = self.current_color[3] / 255.0
        print(f"[VoxelViewport] Placing voxel at ({x}, {y}, {z}) with color ({r:.2f}, {g:.2f}, {b:.2f}, {a:.2f})")
        self.editor_bridge.voxel_builder_place_voxel(self.voxel_builder_id, x, y, z, r, g, b, a)

    def erase_voxel(self, x, y, z):
        """Erase a voxel using C++ backend"""
        if not self.voxel_builder_id or not self.editor_bridge:
            return
        self.editor_bridge.voxel_builder_erase_voxel(self.voxel_builder_id, x, y, z)

    def _complete_shape(self, start, end):
        """Complete a shape tool operation"""
        voxels = []

        if self.edit_mode == EditMode.LINE:
            voxels = self._bresenham_line_3d(start, end)
        elif self.edit_mode == EditMode.RECT:
            x1, y1, z1 = start
            x2, y2, z2 = end
            min_x, max_x = min(x1, x2), max(x1, x2)
            min_y, max_y = min(y1, y2), max(y1, y2)
            min_z, max_z = min(z1, z2), max(z1, z2)

            for x in range(min_x, max_x + 1):
                for y in range(min_y, max_y + 1):
                    for z in range(min_z, max_z + 1):
                        voxels.append((x, y, z))

        elif self.edit_mode == EditMode.SPHERE:
            cx = (start[0] + end[0]) / 2.0
            cy = (start[1] + end[1]) / 2.0
            cz = (start[2] + end[2]) / 2.0
            radius = max(1, int(math.sqrt(
                (end[0] - start[0])**2 +
                (end[1] - start[1])**2 +
                (end[2] - start[2])**2
            ) / 2.0))

            for x in range(int(cx - radius), int(cx + radius) + 1):
                for y in range(int(cy - radius), int(cy + radius) + 1):
                    for z in range(int(cz - radius), int(cz + radius) + 1):
                        dist = math.sqrt((x - cx)**2 + (y - cy)**2 + (z - cz)**2)
                        if dist <= radius:
                            voxels.append((x, y, z))

        # Place all voxels
        for vx, vy, vz in voxels:
            self.place_voxel(vx, vy, vz)

    def _flood_fill(self, x, y, z):
        """Flood fill starting from position"""
        # For now, just place a single voxel
        self.place_voxel(x, y, z)

    def _render_selection_boxes(self):
        """Render bounding boxes around selected voxels"""
        if not self.editor_bridge or not self.view_id:
            return

        # Render a bounding box for each selected voxel
        for voxel_pos in self.selected_voxels:
            x, y, z = voxel_pos
            # Each voxel is a 1x1x1 cube, centered at integer coordinates
            # Calculate AABB bounds (min, max)
            min_x, min_y, min_z = x - 0.5, y - 0.5, z - 0.5
            max_x, max_y, max_z = x + 0.5, y + 0.5, z + 0.5

            # Draw bounding box using debug renderer (orange color for selected)
            try:
                self.editor_bridge.debug_draw_aabb(
                    self.view_id,
                    min_x, min_y, min_z,
                    max_x, max_y, max_z,
                    1.0, 0.6, 0.0, 1.0  # Orange
                )
            except Exception as e:
                pass  # Silently ignore if debug draw not available

    def _render_floor_grid(self):
        """Render a 5x5 grid at the current floor level based on working side"""
        if not self.editor_bridge or not self.view_id or not self.show_grid:
            return

        cx, cy, cz = self.cursor_pos
        floor_level = float(self.grid_floor_y)
        grid_size = 5
        half_grid = grid_size // 2

        try:
            # Draw grid based on working side (which plane we're working on)
            if self.working_side == WorkingSide.FLOOR:
                # XZ plane, Y is constant (floor)
                for z_offset in range(-half_grid, half_grid + 1):
                    z = cz + z_offset
                    x_start, x_end = cx - half_grid, cx + half_grid
                    self.editor_bridge.debug_draw_line(
                        self.view_id,
                        float(x_start), floor_level, float(z),
                        float(x_end), floor_level, float(z),
                        0.5, 0.5, 0.5, 0.5
                    )
                for x_offset in range(-half_grid, half_grid + 1):
                    x = cx + x_offset
                    z_start, z_end = cz - half_grid, cz + half_grid
                    self.editor_bridge.debug_draw_line(
                        self.view_id,
                        float(x), floor_level, float(z_start),
                        float(x), floor_level, float(z_end),
                        0.5, 0.5, 0.5, 0.5
                    )

            elif self.working_side in [WorkingSide.LEFT, WorkingSide.RIGHT]:
                # YZ plane, X is constant (wall)
                for z_offset in range(-half_grid, half_grid + 1):
                    z = cz + z_offset
                    y_start, y_end = cy - half_grid, cy + half_grid
                    self.editor_bridge.debug_draw_line(
                        self.view_id,
                        floor_level, float(y_start), float(z),
                        floor_level, float(y_end), float(z),
                        0.5, 0.5, 0.5, 0.5
                    )
                for y_offset in range(-half_grid, half_grid + 1):
                    y = cy + y_offset
                    z_start, z_end = cz - half_grid, cz + half_grid
                    self.editor_bridge.debug_draw_line(
                        self.view_id,
                        floor_level, float(y), float(z_start),
                        floor_level, float(y), float(z_end),
                        0.5, 0.5, 0.5, 0.5
                    )

            elif self.working_side in [WorkingSide.FORWARD, WorkingSide.BACKWARD]:
                # XY plane, Z is constant (front/back wall)
                for y_offset in range(-half_grid, half_grid + 1):
                    y = cy + y_offset
                    x_start, x_end = cx - half_grid, cx + half_grid
                    self.editor_bridge.debug_draw_line(
                        self.view_id,
                        float(x_start), float(y), floor_level,
                        float(x_end), float(y), floor_level,
                        0.5, 0.5, 0.5, 0.5
                    )
                for x_offset in range(-half_grid, half_grid + 1):
                    x = cx + x_offset
                    y_start, y_end = cy - half_grid, cy + half_grid
                    self.editor_bridge.debug_draw_line(
                        self.view_id,
                        float(x), float(y_start), floor_level,
                        float(x), float(y_end), floor_level,
                        0.5, 0.5, 0.5, 0.5
                    )
        except Exception as e:
            pass  # Silently ignore if debug draw not available

    def _render_floor_indicator(self):
        """Render a small grid at the base of the cursor voxel when floor_y != 0"""
        if not self.editor_bridge or not self.view_id:
            return

        # Only show when floor is not at 0
        if self.grid_floor_y == 0:
            return

        cx, cy, cz = self.cursor_pos
        floor_level = float(self.grid_floor_y)
        half_size = 0.5

        try:
            # Grid color: bright yellow to stand out
            r, g, b, a = 1.0, 1.0, 0.0, 0.8

            # Draw square outline and connecting line based on working side
            if self.working_side == WorkingSide.FLOOR:
                # XZ plane square at floor Y level
                x_min, x_max = cx - half_size, cx + half_size
                z_min, z_max = cz - half_size, cz + half_size

                # Four edges
                self.editor_bridge.debug_draw_line(self.view_id, float(x_min), floor_level, float(z_min), float(x_max), floor_level, float(z_min), r, g, b, a)
                self.editor_bridge.debug_draw_line(self.view_id, float(x_max), floor_level, float(z_min), float(x_max), floor_level, float(z_max), r, g, b, a)
                self.editor_bridge.debug_draw_line(self.view_id, float(x_max), floor_level, float(z_max), float(x_min), floor_level, float(z_max), r, g, b, a)
                self.editor_bridge.debug_draw_line(self.view_id, float(x_min), floor_level, float(z_max), float(x_min), floor_level, float(z_min), r, g, b, a)

                # Vertical line from floor to voxel
                self.editor_bridge.debug_draw_line(self.view_id, float(cx), floor_level, float(cz), float(cx), float(cy), float(cz), r, g, b, a * 0.5)

            elif self.working_side in [WorkingSide.LEFT, WorkingSide.RIGHT]:
                # YZ plane square at floor X level
                y_min, y_max = cy - half_size, cy + half_size
                z_min, z_max = cz - half_size, cz + half_size

                # Four edges
                self.editor_bridge.debug_draw_line(self.view_id, floor_level, float(y_min), float(z_min), floor_level, float(y_max), float(z_min), r, g, b, a)
                self.editor_bridge.debug_draw_line(self.view_id, floor_level, float(y_max), float(z_min), floor_level, float(y_max), float(z_max), r, g, b, a)
                self.editor_bridge.debug_draw_line(self.view_id, floor_level, float(y_max), float(z_max), floor_level, float(y_min), float(z_max), r, g, b, a)
                self.editor_bridge.debug_draw_line(self.view_id, floor_level, float(y_min), float(z_max), floor_level, float(y_min), float(z_min), r, g, b, a)

                # Horizontal line from floor to voxel
                self.editor_bridge.debug_draw_line(self.view_id, floor_level, float(cy), float(cz), float(cx), float(cy), float(cz), r, g, b, a * 0.5)

            elif self.working_side in [WorkingSide.FORWARD, WorkingSide.BACKWARD]:
                # XY plane square at floor Z level
                x_min, x_max = cx - half_size, cx + half_size
                y_min, y_max = cy - half_size, cy + half_size

                # Four edges
                self.editor_bridge.debug_draw_line(self.view_id, float(x_min), float(y_min), floor_level, float(x_max), float(y_min), floor_level, r, g, b, a)
                self.editor_bridge.debug_draw_line(self.view_id, float(x_max), float(y_min), floor_level, float(x_max), float(y_max), floor_level, r, g, b, a)
                self.editor_bridge.debug_draw_line(self.view_id, float(x_max), float(y_max), floor_level, float(x_min), float(y_max), floor_level, r, g, b, a)
                self.editor_bridge.debug_draw_line(self.view_id, float(x_min), float(y_max), floor_level, float(x_min), float(y_min), floor_level, r, g, b, a)

                # Depth line from floor to voxel
                self.editor_bridge.debug_draw_line(self.view_id, float(cx), float(cy), floor_level, float(cx), float(cy), float(cz), r, g, b, a * 0.5)

        except Exception as e:
            pass  # Silently ignore if debug draw not available

    def clear(self):
        """Clear all voxels using C++ backend"""
        if not self.voxel_builder_id or not self.editor_bridge:
            return
        self.editor_bridge.voxel_builder_clear(self.voxel_builder_id)

    def get_voxel_count(self):
        """Get number of voxels"""
        if not self.voxel_builder_id or not self.editor_bridge:
            return 0
        return self.editor_bridge.voxel_builder_get_voxel_count(self.voxel_builder_id)

    def cleanup(self):
        if self.render_timer:
            self.render_timer.stop()
        if self.editor_bridge:
            if self.view_id:
                try:
                    self.editor_bridge.destroy_render_view(self.view_id)
                except:
                    pass
            if self.voxel_builder_id:
                try:
                    self.editor_bridge.destroy_voxel_builder(self.voxel_builder_id)
                except:
                    pass
            if self.ghost_voxel_builder_id:
                try:
                    self.editor_bridge.destroy_voxel_builder(self.ghost_voxel_builder_id)
                except:
                    pass


class VoxelBuilderTool(EditorPanel):
    """Full Voxel Builder tool with tabbed interface"""

    def __init__(self, editor_bridge=None, parent=None, editor_session=None):
        self.editor_bridge = editor_bridge
        self.editor_session = editor_session
        self.viewport = None
        super().__init__("Voxel Builder", parent)
        self.setMinimumSize(800, 600)

    def showEvent(self, event):
        super().showEvent(event)
        if self.viewport and self.editor_bridge and self.editor_session and not self.viewport.view_id:
            QTimer.singleShot(100, self._init_viewport)

    def _init_viewport(self):
        if self.viewport and self.editor_bridge and self.editor_session and not self.viewport.view_id:
            self.viewport.initialize_rendering(self.editor_bridge, self.editor_session)

    def _setup_panel(self):
        theme = get_theme_manager().get_current_theme()
        colors = theme.colors

        main_splitter = QSplitter(Qt.Orientation.Horizontal)

        # Left: Viewport
        self.viewport = VoxelViewport()
        self.viewport.parent_tool = self  # Connect viewport to parent for UI updates
        main_splitter.addWidget(self.viewport)

        # Right: Tabbed tools panel
        tools_container = QWidget()
        tools_container.setMinimumWidth(250)
        tools_layout = QVBoxLayout()
        tools_layout.setContentsMargins(4, 4, 4, 4)
        tools_layout.setSpacing(4)

        # Create tabs
        self.tabs = QTabWidget()
        self.tabs.setTabPosition(QTabWidget.TabPosition.North)

        # Edit tab
        edit_tab = self._create_edit_tab()
        self.tabs.addTab(edit_tab, "Edit")

        # Export tab
        export_tab = self._create_export_tab()
        self.tabs.addTab(export_tab, "Export")

        tools_layout.addWidget(self.tabs)
        tools_container.setLayout(tools_layout)
        main_splitter.addWidget(tools_container)

        main_splitter.setSizes([600, 250])
        self.content_layout.addWidget(main_splitter)

    def _create_edit_tab(self):
        """Create edit tools tab"""
        tab = QWidget()
        layout = QVBoxLayout()

        # Edit mode
        mode_group = QGroupBox("Edit Mode")
        mode_layout = QVBoxLayout()

        self.mode_buttons = QButtonGroup()
        modes = [
            ("Place", EditMode.PLACE),
            ("Erase", EditMode.ERASE),
            ("Select", EditMode.SELECT),
            ("Fill", EditMode.FILL),
            ("Line", EditMode.LINE),
            ("Rect", EditMode.RECT),
            ("Sphere", EditMode.SPHERE),
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

        # Placement mode
        placement_group = QGroupBox("Placement")
        placement_layout = QVBoxLayout()

        self.placement_combo = QComboBox()
        self.placement_combo.addItem("Snap to Grid", PlacementMode.SNAP_GRID)
        self.placement_combo.addItem("Snap to Face", PlacementMode.SNAP_FACE)
        self.placement_combo.addItem("Free Place", PlacementMode.FREE)
        self.placement_combo.currentIndexChanged.connect(self._set_placement_mode)

        placement_layout.addWidget(self.placement_combo)
        placement_group.setLayout(placement_layout)
        layout.addWidget(placement_group)

        # Color picker
        color_group = QGroupBox("Color")
        color_layout = QVBoxLayout()

        color_btn_layout = QHBoxLayout()
        self.color_preview = QWidget()
        self.color_preview.setFixedSize(40, 40)
        self.color_preview.setAutoFillBackground(True)
        self._update_color_preview()

        color_btn = QPushButton("Pick Color")
        color_btn.clicked.connect(self._pick_color)

        color_btn_layout.addWidget(self.color_preview)
        color_btn_layout.addWidget(color_btn)
        color_btn_layout.addStretch()

        # Common colors palette
        palette_layout = QHBoxLayout()
        common_colors = [
            (255, 0, 0, 255),    # Red
            (0, 255, 0, 255),    # Green
            (0, 0, 255, 255),    # Blue
            (255, 255, 0, 255),  # Yellow
            (255, 128, 0, 255),  # Orange
            (128, 0, 255, 255),  # Purple
            (255, 255, 255, 255),# White
            (128, 128, 128, 255),# Gray
        ]

        for color in common_colors:
            btn = QPushButton()
            btn.setFixedSize(20, 20)
            btn.setStyleSheet(f"background-color: rgb({color[0]}, {color[1]}, {color[2]}); border: 1px solid #555;")
            btn.clicked.connect(lambda checked, c=color: self._set_color(c))
            palette_layout.addWidget(btn)

        palette_layout.addStretch()

        color_layout.addLayout(color_btn_layout)
        color_layout.addLayout(palette_layout)
        color_group.setLayout(color_layout)
        layout.addWidget(color_group)

        # Grid settings (moved from Properties tab)
        grid_group = QGroupBox("Grid")
        grid_layout = QFormLayout()

        self.floor_spin = QSpinBox()
        self.floor_spin.setRange(-100, 100)
        self.floor_spin.setValue(0)
        self.floor_spin.valueChanged.connect(self._set_floor_y)

        self.side_combo = QComboBox()
        self.side_combo.addItem("Floor (XZ, Y up)", WorkingSide.FLOOR)
        self.side_combo.addItem("Left (YZ, X up)", WorkingSide.LEFT)
        self.side_combo.addItem("Right (YZ, -X up)", WorkingSide.RIGHT)
        self.side_combo.addItem("Forward (XY, Z up)", WorkingSide.FORWARD)
        self.side_combo.addItem("Backward (XY, -Z up)", WorkingSide.BACKWARD)
        self.side_combo.currentIndexChanged.connect(self._set_working_side)

        self.ghost_check = QCheckBox("Show Ghost")
        self.ghost_check.setChecked(True)
        self.ghost_check.toggled.connect(lambda checked: setattr(self.viewport, 'show_ghost', checked))

        self.grid_check = QCheckBox("Show Grid")
        self.grid_check.setChecked(True)
        self.grid_check.toggled.connect(lambda checked: setattr(self.viewport, 'show_grid', checked))

        grid_layout.addRow("Floor Y:", self.floor_spin)
        grid_layout.addRow("Side:", self.side_combo)
        grid_layout.addRow(self.ghost_check)
        grid_layout.addRow(self.grid_check)
        grid_group.setLayout(grid_layout)
        layout.addWidget(grid_group)

        # Stats (moved from Properties tab)
        stats_group = QGroupBox("Stats")
        stats_layout = QVBoxLayout()

        self.voxel_count_label = QLabel("Voxels: 0")
        self.selected_count_label = QLabel("Selected: 0")
        stats_layout.addWidget(self.voxel_count_label)
        stats_layout.addWidget(self.selected_count_label)

        # Clear selection button
        clear_selection_btn = QPushButton("Clear Selection")
        clear_selection_btn.clicked.connect(self._clear_selection)
        stats_layout.addWidget(clear_selection_btn)

        stats_group.setLayout(stats_layout)
        layout.addWidget(stats_group)

        # Update stats timer
        self.stats_timer = QTimer(self)
        self.stats_timer.timeout.connect(self._update_stats)
        self.stats_timer.start(500)

        layout.addStretch()
        tab.setLayout(layout)
        return tab


    def _create_export_tab(self):
        """Create export tab"""
        tab = QWidget()
        layout = QVBoxLayout()

        # File operations
        file_group = QGroupBox("File Operations")
        file_layout = QVBoxLayout()

        new_btn = QPushButton("New")
        new_btn.clicked.connect(self._new_scene)
        file_layout.addWidget(new_btn)

        save_btn = QPushButton("Save .vox")
        save_btn.clicked.connect(self._save_vox)
        file_layout.addWidget(save_btn)

        load_btn = QPushButton("Load .vox")
        load_btn.clicked.connect(self._load_vox)
        file_layout.addWidget(load_btn)

        file_group.setLayout(file_layout)
        layout.addWidget(file_group)

        # Export options
        export_options_group = QGroupBox("Export Options")
        export_options_layout = QVBoxLayout()

        # Mesh optimization
        self.merge_faces_check = QCheckBox("Merge Internal Faces")
        self.merge_faces_check.setChecked(True)
        self.merge_faces_check.setToolTip("Merge internal faces to show only external visible faces")
        export_options_layout.addWidget(self.merge_faces_check)

        # Texture mode
        texture_label = QLabel("Texture Mode:")
        export_options_layout.addWidget(texture_label)

        self.texture_mode_group = QButtonGroup()

        self.texture_atlas_radio = QRadioButton("Texture Atlas")
        self.texture_atlas_radio.setChecked(True)
        self.texture_atlas_radio.setToolTip("Create 1x1 color squares and atlas them")
        self.texture_mode_group.addButton(self.texture_atlas_radio, 0)
        export_options_layout.addWidget(self.texture_atlas_radio)

        self.texture_wrap_radio = QRadioButton("Full Unwrap")
        self.texture_wrap_radio.setToolTip("Smart UV unwrapping with UV islands per face")
        self.texture_mode_group.addButton(self.texture_wrap_radio, 1)
        export_options_layout.addWidget(self.texture_wrap_radio)

        export_options_group.setLayout(export_options_layout)
        layout.addWidget(export_options_group)

        # Export buttons
        export_group = QGroupBox("Export")
        export_layout = QVBoxLayout()

        export_obj_btn = QPushButton("Export to OBJ")
        export_obj_btn.clicked.connect(self._export_obj)
        export_layout.addWidget(export_obj_btn)

        export_gltf_btn = QPushButton("Export to glTF")
        export_gltf_btn.clicked.connect(self._export_gltf)
        export_layout.addWidget(export_gltf_btn)

        export_group.setLayout(export_layout)
        layout.addWidget(export_group)

        layout.addStretch()
        tab.setLayout(layout)
        return tab

    def _set_edit_mode(self, mode):
        if self.viewport:
            self.viewport.edit_mode = mode

    def _set_placement_mode(self, index):
        if self.viewport:
            self.viewport.placement_mode = self.placement_combo.itemData(index)

    def _set_floor_y(self, value):
        if self.viewport:
            self.viewport.grid_floor_y = value

    def _set_working_side(self, index):
        if self.viewport:
            self.viewport.working_side = self.side_combo.itemData(index)
            print(f"[VoxelBuilder] Working side: {self.viewport.working_side.value}")

    def _clear_selection(self):
        if self.viewport:
            self.viewport.selected_voxels.clear()

    def _set_color(self, color):
        if self.viewport:
            self.viewport.current_color = color
            self._update_color_preview()

    def _pick_color(self):
        current = QColor(*self.viewport.current_color[:3])
        color = QColorDialog.getColor(current, self, "Pick Color")
        if color.isValid():
            self.viewport.current_color = (color.red(), color.green(), color.blue(), 255)
            self._update_color_preview()

    def _update_color_preview(self):
        palette = self.color_preview.palette()
        palette.setColor(QPalette.ColorRole.Window, QColor(*self.viewport.current_color[:3]))
        self.color_preview.setPalette(palette)

    def _update_stats(self):
        if self.viewport:
            count = self.viewport.get_voxel_count()
            selected_count = len(self.viewport.selected_voxels)
            self.voxel_count_label.setText(f"Voxels: {count}")
            self.selected_count_label.setText(f"Selected: {selected_count}")

    def _new_scene(self):
        reply = QMessageBox.question(self, "New", "Clear all voxels?",
                                     QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No)
        if reply == QMessageBox.StandardButton.Yes:
            self.viewport.clear()

    def _save_vox(self):
        path, _ = QFileDialog.getSaveFileName(self, "Save", "", "Voxel Files (*.vox)")
        if path:
            try:
                # Normalize path to use backslashes on Windows
                path = path.replace("/", "\\")
                json_data = self.viewport.editor_bridge.voxel_builder_to_json(self.viewport.voxel_builder_id)
                with open(path, 'w') as f:
                    f.write(json_data)
                QMessageBox.information(self, "Success", f"Saved to:\n{path}")
            except Exception as e:
                QMessageBox.critical(self, "Error", str(e))

    def _load_vox(self):
        path, _ = QFileDialog.getOpenFileName(self, "Load", "", "Voxel Files (*.vox)")
        if path:
            try:
                # Normalize path to use backslashes on Windows
                path = path.replace("/", "\\")
                with open(path, 'r') as f:
                    json_data = f.read()
                self.viewport.editor_bridge.voxel_builder_from_json(self.viewport.voxel_builder_id, json_data)
                QMessageBox.information(self, "Success", f"Loaded from:\n{path}")
            except Exception as e:
                QMessageBox.critical(self, "Error", str(e))

    def _export_obj(self):
        path, _ = QFileDialog.getSaveFileName(self, "Export OBJ", "", "OBJ Files (*.obj)")
        if path:
            try:
                # Normalize path to use backslashes on Windows
                path = path.replace("/", "\\")
                # Get export options
                merge_faces = self.merge_faces_check.isChecked()
                texture_atlas = self.texture_atlas_radio.isChecked()

                # Export using C++ backend with options
                obj_data = self.viewport.editor_bridge.voxel_builder_export_obj(
                    self.viewport.voxel_builder_id,
                    merge_faces,
                    texture_atlas
                )
                with open(path, 'w') as f:
                    f.write(obj_data)

                options_str = []
                if merge_faces:
                    options_str.append("Merged internal faces")
                if texture_atlas:
                    options_str.append("Texture atlas mode")
                options_text = "\n".join(options_str) if options_str else "No special options"

                QMessageBox.information(self, "Success", f"Exported to OBJ!\n\n{path}\n\n{options_text}")
            except Exception as e:
                QMessageBox.critical(self, "Error", str(e))

    def _export_gltf(self):
        path, _ = QFileDialog.getSaveFileName(self, "Export glTF", "", "glTF Files (*.gltf *.glb)")
        if path:
            try:
                # Normalize path to use backslashes on Windows
                path = path.replace("/", "\\")
                # Get export options
                merge_faces = self.merge_faces_check.isChecked()
                texture_atlas = self.texture_atlas_radio.isChecked()

                # Export using C++ backend with options
                gltf_data = self.viewport.editor_bridge.voxel_builder_export_gltf(
                    self.viewport.voxel_builder_id,
                    merge_faces,
                    texture_atlas
                )
                with open(path, 'w') as f:
                    f.write(gltf_data)

                options_str = []
                if merge_faces:
                    options_str.append("Merged internal faces")
                if texture_atlas:
                    options_str.append("Texture atlas mode")
                else:
                    options_str.append("Vertex colors")
                options_text = "\n".join(options_str) if options_str else "No special options"

                QMessageBox.information(self, "Success", f"Exported to glTF!\n\n{path}\n\n{options_text}")
            except Exception as e:
                QMessageBox.critical(self, "Error", str(e))

    def closeEvent(self, event):
        if hasattr(self, 'stats_timer') and self.stats_timer:
            self.stats_timer.stop()
        self.viewport.cleanup()
        super().closeEvent(event)
