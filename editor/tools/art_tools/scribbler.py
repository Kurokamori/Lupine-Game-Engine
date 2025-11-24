"""
Scribbler Art Tool
A simple art program for creating placeholder drawings and visual assets
Features: Brush, Eraser, Layers with blend modes, Color wheel, Pen pressure support
Undo/Redo, Copy/Paste/Cut support, External image paste
"""

from PyQt6.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QLabel,
                             QPushButton, QListWidget, QListWidgetItem,
                             QFileDialog, QMessageBox, QDialog, QFormLayout,
                             QLineEdit, QComboBox, QDialogButtonBox, QSpinBox,
                             QDoubleSpinBox, QSplitter, QGroupBox, QGridLayout,
                             QScrollArea, QRadioButton, QButtonGroup, QFrame,
                             QSlider, QColorDialog, QCheckBox, QMenuBar, QMenu,
                             QInputDialog)
from PyQt6.QtCore import Qt, QTimer, pyqtSignal, QSize, QRect, QPoint, QPointF, QMimeData, QRectF
from PyQt6.QtGui import (QPixmap, QImage, QPainter, QColor, QPen, QBrush,
                        QPainterPath, QTransform, QTabletEvent, QPalette,
                        QKeySequence, QAction, QShortcut, QClipboard, QBitmap, QRegion)
from pathlib import Path
import json
import sys
import uuid
import math
from abc import ABC, abstractmethod
from typing import List, Optional

# Import base panel
editor_path = Path(__file__).parent.parent.parent
if str(editor_path) not in sys.path:
    sys.path.insert(0, str(editor_path))

from panels.base_panel import EditorPanel
from theme import get_theme_manager
from tools.art_tools.timeline_widget import TimelineWidget


# ============================================================================
# Command Pattern for Undo/Redo
# ============================================================================

class Command(ABC):
    """Base class for commands"""

    @abstractmethod
    def execute(self):
        """Execute the command"""
        pass

    @abstractmethod
    def undo(self):
        """Undo the command"""
        pass


class DrawCommand(Command):
    """Command for drawing operations"""

    def __init__(self, layer: 'Layer', before_state: QPixmap):
        self.layer = layer
        self.before_state = before_state.copy()
        self.after_state = None

    def execute(self):
        """Execute - store after state"""
        self.after_state = self.layer.canvas.copy()

    def undo(self):
        """Undo drawing"""
        self.layer.canvas = self.before_state.copy()

    def redo(self):
        """Redo drawing"""
        if self.after_state:
            self.layer.canvas = self.after_state.copy()


class AddLayerCommand(Command):
    """Command for adding a layer"""

    def __init__(self, canvas: 'CanvasWidget', layer: 'Layer', index: int):
        self.canvas = canvas
        self.layer = layer
        self.index = index

    def execute(self):
        """Add layer"""
        self.canvas.layers.insert(self.index, self.layer)
        self.canvas.current_layer_index = self.index

    def undo(self):
        """Remove layer"""
        if self.index < len(self.canvas.layers):
            self.canvas.layers.pop(self.index)
            if self.canvas.current_layer_index >= len(self.canvas.layers):
                self.canvas.current_layer_index = len(self.canvas.layers) - 1


class RemoveLayerCommand(Command):
    """Command for removing a layer"""

    def __init__(self, canvas: 'CanvasWidget', layer: 'Layer', index: int):
        self.canvas = canvas
        self.layer = layer.copy()  # Store a copy
        self.index = index
        self.prev_current_index = canvas.current_layer_index

    def execute(self):
        """Remove layer"""
        if self.index < len(self.canvas.layers):
            self.canvas.layers.pop(self.index)
            if self.canvas.current_layer_index >= len(self.canvas.layers):
                self.canvas.current_layer_index = len(self.canvas.layers) - 1

    def undo(self):
        """Re-add layer"""
        self.canvas.layers.insert(self.index, self.layer)
        self.canvas.current_layer_index = self.prev_current_index


class ModifyLayerCommand(Command):
    """Command for modifying layer properties"""

    def __init__(self, layer: 'Layer'):
        self.layer = layer
        self.before_state = {
            'visible': layer.visible,
            'opacity': layer.opacity,
            'blend_mode': layer.blend_mode,
            'locked': layer.locked,
            'name': layer.name
        }
        self.after_state = None

    def execute(self):
        """Store after state"""
        self.after_state = {
            'visible': self.layer.visible,
            'opacity': self.layer.opacity,
            'blend_mode': self.layer.blend_mode,
            'locked': self.layer.locked,
            'name': self.layer.name
        }

    def undo(self):
        """Restore before state"""
        self.layer.visible = self.before_state['visible']
        self.layer.opacity = self.before_state['opacity']
        self.layer.blend_mode = self.before_state['blend_mode']
        self.layer.locked = self.before_state['locked']
        self.layer.name = self.before_state['name']

    def redo(self):
        """Restore after state"""
        if self.after_state:
            self.layer.visible = self.after_state['visible']
            self.layer.opacity = self.after_state['opacity']
            self.layer.blend_mode = self.after_state['blend_mode']
            self.layer.locked = self.after_state['locked']
            self.layer.name = self.after_state['name']


class PasteLayerCommand(Command):
    """Command for pasting a layer"""

    def __init__(self, canvas: 'CanvasWidget', layer: 'Layer'):
        self.canvas = canvas
        self.layer = layer
        self.index = None

    def execute(self):
        """Paste layer"""
        self.index = self.canvas.current_layer_index + 1
        self.canvas.layers.insert(self.index, self.layer)
        self.canvas.current_layer_index = self.index

    def undo(self):
        """Remove pasted layer"""
        if self.index is not None and self.index < len(self.canvas.layers):
            self.canvas.layers.pop(self.index)
            if self.canvas.current_layer_index >= len(self.canvas.layers):
                self.canvas.current_layer_index = len(self.canvas.layers) - 1


class MoveLayerCommand(Command):
    """Command for moving/transforming a layer"""

    def __init__(self, layer: 'Layer', before_canvas: QPixmap):
        self.layer = layer
        self.before_canvas = before_canvas.copy()
        self.after_canvas = None

    def execute(self):
        """Store after state"""
        self.after_canvas = self.layer.canvas.copy()

    def undo(self):
        """Restore old canvas"""
        self.layer.canvas = self.before_canvas.copy()

    def redo(self):
        """Restore new canvas"""
        if self.after_canvas is not None:
            self.layer.canvas = self.after_canvas.copy()


class CommandHistory:
    """Manages command history for undo/redo"""

    def __init__(self, max_history: int = 50):
        self.max_history = max_history
        self.undo_stack: List[Command] = []
        self.redo_stack: List[Command] = []

    def execute(self, command: Command):
        """Execute a command and add to history"""
        command.execute()
        self.undo_stack.append(command)

        # Clear redo stack when new command is executed
        self.redo_stack.clear()

        # Limit history size
        if len(self.undo_stack) > self.max_history:
            self.undo_stack.pop(0)

    def undo(self) -> bool:
        """Undo last command"""
        if not self.undo_stack:
            return False

        command = self.undo_stack.pop()
        command.undo()
        self.redo_stack.append(command)
        return True

    def redo(self) -> bool:
        """Redo last undone command"""
        if not self.redo_stack:
            return False

        command = self.redo_stack.pop()

        # Handle commands with redo method
        if hasattr(command, 'redo'):
            command.redo()
        else:
            command.execute()

        self.undo_stack.append(command)
        return True

    def can_undo(self) -> bool:
        """Check if undo is available"""
        return len(self.undo_stack) > 0

    def can_redo(self) -> bool:
        """Check if redo is available"""
        return len(self.redo_stack) > 0

    def clear(self):
        """Clear all history"""
        self.undo_stack.clear()
        self.redo_stack.clear()


class BlendMode:
    """Blend modes for layers"""
    NORMAL = "normal"
    MULTIPLY = "multiply"
    OVERLAY = "overlay"
    SOFTLIGHT = "softlight"

    @staticmethod
    def get_all():
        return [BlendMode.NORMAL, BlendMode.MULTIPLY, BlendMode.OVERLAY, BlendMode.SOFTLIGHT]


class Frame:
    """Represents a single animation frame in a layer"""
    def __init__(self, width: int, height: int):
        self.canvas = QPixmap(width, height)
        self.canvas.fill(Qt.GlobalColor.transparent)
        self.duration = 100  # Duration in milliseconds


class AnimationTag:
    """Represents a named animation sequence"""
    def __init__(self, name: str, start_frame: int, end_frame: int):
        self.name = name
        self.start_frame = start_frame
        self.end_frame = end_frame
        self.direction = "forward"  # "forward", "reverse", "pingpong"


class Layer:
    """Represents a drawing layer with animation support"""
    def __init__(self, name: str, width: int, height: int):
        self.name = name
        self.width = width
        self.height = height
        self.frames = [Frame(width, height)]  # Animation frames
        self._current_frame_index = 0
        self.visible = True
        self.opacity = 1.0
        self.blend_mode = BlendMode.NORMAL
        self.locked = False
        self.offset_x = 0  # Layer position offset
        self.offset_y = 0

    @property
    def canvas(self):
        """Get current frame's canvas"""
        return self.frames[self._current_frame_index].canvas

    @canvas.setter
    def canvas(self, value):
        """Set current frame's canvas"""
        self.frames[self._current_frame_index].canvas = value

    def resize(self, width: int, height: int):
        """Resize all frames in the layer"""
        self.width = width
        self.height = height
        for frame in self.frames:
            new_canvas = QPixmap(width, height)
            new_canvas.fill(Qt.GlobalColor.transparent)
            painter = QPainter(new_canvas)
            painter.drawPixmap(0, 0, frame.canvas)
            painter.end()
            frame.canvas = new_canvas

    def clear(self):
        """Clear the layer"""
        self.canvas.fill(Qt.GlobalColor.transparent)

    def copy(self):
        """Create a copy of this layer"""
        new_layer = Layer(f"{self.name} Copy", self.width, self.height)
        # Copy all frames
        new_layer.frames = []
        for frame in self.frames:
            new_frame = Frame(self.width, self.height)
            new_frame.canvas = frame.canvas.copy()
            new_frame.duration = frame.duration
            new_layer.frames.append(new_frame)
        new_layer._current_frame_index = self._current_frame_index
        new_layer.visible = self.visible
        new_layer.opacity = self.opacity
        new_layer.blend_mode = self.blend_mode
        new_layer.locked = self.locked
        new_layer.offset_x = self.offset_x
        new_layer.offset_y = self.offset_y
        return new_layer

    def add_frame(self, index: int = None):
        """Add a new frame at the specified index (or at the end)"""
        new_frame = Frame(self.width, self.height)
        if index is None:
            self.frames.append(new_frame)
        else:
            self.frames.insert(index, new_frame)

    def duplicate_frame(self, index: int):
        """Duplicate the frame at the specified index"""
        if 0 <= index < len(self.frames):
            original = self.frames[index]
            new_frame = Frame(self.width, self.height)
            new_frame.canvas = original.canvas.copy()
            new_frame.duration = original.duration
            self.frames.insert(index + 1, new_frame)

    def delete_frame(self, index: int):
        """Delete the frame at the specified index"""
        if len(self.frames) > 1 and 0 <= index < len(self.frames):
            del self.frames[index]
            if self._current_frame_index >= len(self.frames):
                self._current_frame_index = len(self.frames) - 1

    def move_frame(self, from_index: int, to_index: int):
        """Move a frame from one index to another"""
        if 0 <= from_index < len(self.frames) and 0 <= to_index < len(self.frames):
            frame = self.frames.pop(from_index)
            self.frames.insert(to_index, frame)


class CanvasWidget(QWidget):
    """
    Canvas widget for drawing
    Cross-platform graphics abstraction using QPixmap/QPainter
    """

    canvas_modified = pyqtSignal()
    history_changed = pyqtSignal()  # Emitted when undo/redo state changes

    def __init__(self, width: int = 100, height: int = 100, bg_color: QColor = None, parent=None):
        super().__init__(parent)

        # Canvas properties
        self.canvas_width = width
        self.canvas_height = height
        self.bg_color = bg_color if bg_color else QColor(200, 200, 200)
        self.has_background = True

        # Layers
        self.layers = []
        self.current_layer_index = 0

        # Animation
        self.current_frame = 0
        self.animation_tags = []  # List of AnimationTag objects
        self.onion_skinning_enabled = False
        self.onion_skin_before = 1  # Number of frames to show before
        self.onion_skin_after = 1   # Number of frames to show after

        # Command history for undo/redo
        self.command_history = CommandHistory()

        # Drawing state for command tracking
        self.drawing_command = None
        self.layer_before_drawing = None

        # Create initial layer
        self.add_layer("Layer 1")

        # View properties
        self.zoom_level = 1.0
        self.pan_offset = QPoint(0, 0)

        # Drawing state
        self.is_drawing = False
        self.last_point = None
        self.current_tool = "brush"  # "brush", "eraser", "move", "lasso", "marquee", "fill"

        # Move tool state
        self.is_moving_layer = False
        self.move_start_pos = None
        self.layer_start_offset = None

        # Selection state
        self.selection_mask = None  # QImage storing the selection (white = selected)
        self.is_selecting = False
        self.selection_points = []  # For lasso tool
        self.selection_start = None  # For marquee tool
        self.shift_pressed = False  # For multi-selection

        # Brush properties
        self.brush_size = 5
        self.brush_opacity = 1.0
        self.brush_color = QColor(0, 0, 0)
        self.brush_antialias = True

        # Pen pressure settings
        self.use_pressure_size = True
        self.use_pressure_opacity = True
        self.current_pressure = 1.0

        # Stabilization settings
        self.stabilization_amount = 0  # 0-100
        self.stabilization_buffer = []  # Buffer of recent points for smoothing

        # Fill bucket settings
        self.fill_tolerance = 0  # 0-255 color tolerance
        self.fill_close_gaps = 0  # 0-10 pixel gap closing

        # UI setup
        self.setMinimumSize(400, 400)
        self.setMouseTracking(True)
        self.setFocusPolicy(Qt.FocusPolicy.StrongFocus)

        # Enable tablet events for pen pressure
        self.setAttribute(Qt.WidgetAttribute.WA_TabletTracking, True)

        # Set background using theme
        theme_manager = get_theme_manager()
        theme = theme_manager.get_current_theme()
        palette = self.palette()
        palette.setColor(QPalette.ColorRole.Window, QColor(theme.colors.surface))
        self.setPalette(palette)
        self.setAutoFillBackground(True)

    def add_layer(self, name: str) -> Layer:
        """Add a new layer"""
        layer = Layer(name, self.canvas_width, self.canvas_height)
        self.layers.append(layer)
        self.current_layer_index = len(self.layers) - 1
        self.update()
        return layer

    def remove_layer(self, index: int):
        """Remove a layer"""
        if len(self.layers) > 1 and 0 <= index < len(self.layers):
            self.layers.pop(index)
            if self.current_layer_index >= len(self.layers):
                self.current_layer_index = len(self.layers) - 1
            self.update()

    def move_layer(self, from_index: int, to_index: int):
        """Move a layer"""
        if 0 <= from_index < len(self.layers) and 0 <= to_index < len(self.layers):
            layer = self.layers.pop(from_index)
            self.layers.insert(to_index, layer)
            if self.current_layer_index == from_index:
                self.current_layer_index = to_index
            self.update()

    def get_current_layer(self) -> Layer:
        """Get the currently active layer"""
        if 0 <= self.current_layer_index < len(self.layers):
            return self.layers[self.current_layer_index]
        return None

    def set_canvas_size(self, width: int, height: int, bg_color: QColor = None):
        """Set canvas size and background"""
        self.canvas_width = width
        self.canvas_height = height
        if bg_color is not None:
            self.bg_color = bg_color

        # Resize all layers
        for layer in self.layers:
            layer.resize(width, height)

        self.update()

    def set_background_enabled(self, enabled: bool):
        """Enable or disable background"""
        self.has_background = enabled
        self.update()

    # Frame management
    def get_frame_count(self) -> int:
        """Get the maximum number of frames across all layers"""
        if not self.layers:
            return 1
        return max(len(layer.frames) for layer in self.layers)

    def set_current_frame(self, frame_index: int):
        """Set the current frame for all layers"""
        if frame_index >= 0:
            self.current_frame = frame_index
            # Update all layers to use this frame
            for layer in self.layers:
                if frame_index < len(layer.frames):
                    layer._current_frame_index = frame_index
                else:
                    # If this layer doesn't have this frame yet, use the last available
                    layer._current_frame_index = len(layer.frames) - 1
            self.update()

    def add_frame_to_all_layers(self, index: int = None):
        """Add a new frame to all layers at the specified index"""
        for layer in self.layers:
            layer.add_frame(index)
        if index is not None and index <= self.current_frame:
            self.current_frame += 1
        self.update()

    def duplicate_current_frame(self):
        """Duplicate the current frame in all layers"""
        for layer in self.layers:
            layer.duplicate_frame(self.current_frame)
        self.current_frame += 1
        self.update()

    def delete_current_frame(self):
        """Delete the current frame from all layers"""
        if self.get_frame_count() > 1:
            for layer in self.layers:
                layer.delete_frame(self.current_frame)
            if self.current_frame >= self.get_frame_count():
                self.current_frame = self.get_frame_count() - 1
            self.update()

    def move_frame(self, from_index: int, to_index: int):
        """Move a frame in all layers"""
        for layer in self.layers:
            layer.move_frame(from_index, to_index)
        if self.current_frame == from_index:
            self.current_frame = to_index
        self.update()

    # Animation tag management
    def add_animation_tag(self, name: str, start_frame: int, end_frame: int):
        """Add a new animation tag"""
        tag = AnimationTag(name, start_frame, end_frame)
        self.animation_tags.append(tag)
        return tag

    def remove_animation_tag(self, tag: AnimationTag):
        """Remove an animation tag"""
        if tag in self.animation_tags:
            self.animation_tags.remove(tag)

    def get_animation_tag(self, name: str) -> AnimationTag:
        """Get an animation tag by name"""
        for tag in self.animation_tags:
            if tag.name == name:
                return tag
        return None

    def composite_layers(self, include_background: bool = True) -> QPixmap:
        """Composite all layers into a single pixmap"""
        result = QPixmap(self.canvas_width, self.canvas_height)

        # Fill with background or transparent
        if include_background and self.has_background:
            result.fill(self.bg_color)
        else:
            result.fill(Qt.GlobalColor.transparent)

        painter = QPainter(result)

        # Draw layers from bottom to top (reverse order since layers[0] is top)
        for layer in reversed(self.layers):
            if not layer.visible:
                continue

            # Set opacity
            painter.setOpacity(layer.opacity)

            # Set composition mode based on blend mode
            if layer.blend_mode == BlendMode.MULTIPLY:
                painter.setCompositionMode(QPainter.CompositionMode.CompositionMode_Multiply)
            elif layer.blend_mode == BlendMode.OVERLAY:
                painter.setCompositionMode(QPainter.CompositionMode.CompositionMode_Overlay)
            elif layer.blend_mode == BlendMode.SOFTLIGHT:
                painter.setCompositionMode(QPainter.CompositionMode.CompositionMode_SoftLight)
            else:  # NORMAL
                painter.setCompositionMode(QPainter.CompositionMode.CompositionMode_SourceOver)

            # Draw layer with offset
            painter.drawPixmap(layer.offset_x, layer.offset_y, layer.canvas)

        painter.end()
        return result

    def paintEvent(self, event):
        """Paint the canvas"""
        # Get theme colors
        theme_manager = get_theme_manager()
        theme = theme_manager.get_current_theme()
        c = theme.colors

        painter = QPainter(self)
        painter.fillRect(self.rect(), QColor(c.surface))

        # Calculate canvas position (centered)
        scaled_width = int(self.canvas_width * self.zoom_level)
        scaled_height = int(self.canvas_height * self.zoom_level)

        canvas_x = (self.width() - scaled_width) // 2 + self.pan_offset.x()
        canvas_y = (self.height() - scaled_height) // 2 + self.pan_offset.y()

        # Draw background layer FIRST if enabled
        if self.has_background:
            painter.fillRect(canvas_x, canvas_y, scaled_width, scaled_height, self.bg_color)
        else:
            # Draw checkerboard pattern for transparency
            self._draw_checkerboard(painter, canvas_x, canvas_y, scaled_width, scaled_height)

        # Draw onion skinning (previous/next frames) on top of background
        if self.onion_skinning_enabled:
            self._draw_onion_skin(painter, canvas_x, canvas_y, scaled_width, scaled_height)

        # Composite and draw layers (current frame) WITHOUT background
        composite = self.composite_layers(include_background=False)

        # Scale canvas
        if self.zoom_level != 1.0:
            composite = composite.scaled(scaled_width, scaled_height,
                                        Qt.AspectRatioMode.KeepAspectRatio,
                                        Qt.TransformationMode.SmoothTransformation)

        # Draw current frame - blend on top of onion skin
        painter.setCompositionMode(QPainter.CompositionMode.CompositionMode_SourceOver)
        painter.setOpacity(1.0)
        painter.drawPixmap(canvas_x, canvas_y, composite)

        # Draw border
        painter.setPen(QPen(QColor(c.border), 1))
        painter.drawRect(canvas_x, canvas_y, scaled_width, scaled_height)

        # Draw bounding box for current layer if in move mode
        if self.current_tool == "move":
            layer = self.get_current_layer()
            if layer and layer.visible:
                # Calculate layer bounds in screen space
                layer_x = int((layer.offset_x) * self.zoom_level) + canvas_x
                layer_y = int((layer.offset_y) * self.zoom_level) + canvas_y
                layer_w = int(layer.canvas.width() * self.zoom_level)
                layer_h = int(layer.canvas.height() * self.zoom_level)

                # Draw bounding box using theme accent color
                painter.setPen(QPen(QColor(c.accent_color), 2, Qt.PenStyle.DashLine))
                painter.drawRect(layer_x, layer_y, layer_w, layer_h)

                # Draw corner handles
                handle_size = 8
                painter.setBrush(QBrush(QColor(c.accent_color)))
                painter.setPen(QPen(QColor(c.text_on_accent), 1))

                # Top-left
                painter.drawRect(layer_x - handle_size // 2, layer_y - handle_size // 2, handle_size, handle_size)
                # Top-right
                painter.drawRect(layer_x + layer_w - handle_size // 2, layer_y - handle_size // 2, handle_size, handle_size)
                # Bottom-left
                painter.drawRect(layer_x - handle_size // 2, layer_y + layer_h - handle_size // 2, handle_size, handle_size)
                # Bottom-right
                painter.drawRect(layer_x + layer_w - handle_size // 2, layer_y + layer_h - handle_size // 2, handle_size, handle_size)

        # Draw selection visualization (marching ants) - optimized version
        if self.selection_mask is not None:
            # Create an inverted mask overlay (darkens non-selected areas)
            overlay = QPixmap(self.canvas_width, self.canvas_height)
            overlay.fill(Qt.GlobalColor.transparent)

            overlay_painter = QPainter(overlay)
            # Fill with semi-transparent black
            overlay_painter.fillRect(0, 0, self.canvas_width, self.canvas_height, QColor(0, 0, 0, 64))

            # Use DestinationOut to clear the overlay where selection exists
            overlay_painter.setCompositionMode(QPainter.CompositionMode.CompositionMode_DestinationOut)
            overlay_painter.drawImage(0, 0, self.selection_mask)
            overlay_painter.end()

            # Scale and draw the overlay
            if self.zoom_level != 1.0:
                overlay = overlay.scaled(scaled_width, scaled_height,
                                        Qt.AspectRatioMode.KeepAspectRatio,
                                        Qt.TransformationMode.FastTransformation)
            painter.drawPixmap(canvas_x, canvas_y, overlay)

            # Draw marching ants border around selected region
            painter.setPen(QPen(QColor(c.accent_color), 2, Qt.PenStyle.DashLine))
            painter.drawRect(canvas_x, canvas_y, scaled_width, scaled_height)

        # Draw selection preview for lasso/marquee tools
        if self.is_selecting:
            if self.current_tool == "lasso" and len(self.selection_points) > 1:
                # Draw lasso preview
                painter.setPen(QPen(QColor(c.accent_color), 2))
                path = QPainterPath()
                first_point = self.selection_points[0]
                screen_first = QPointF(
                    canvas_x + first_point.x() * self.zoom_level,
                    canvas_y + first_point.y() * self.zoom_level
                )
                path.moveTo(screen_first)
                for point in self.selection_points[1:]:
                    screen_point = QPointF(
                        canvas_x + point.x() * self.zoom_level,
                        canvas_y + point.y() * self.zoom_level
                    )
                    path.lineTo(screen_point)
                painter.drawPath(path)
            elif self.current_tool == "marquee" and self.selection_start:
                # Draw marquee preview
                painter.setPen(QPen(QColor(c.accent_color), 2, Qt.PenStyle.DashLine))
                start_screen = QPointF(
                    canvas_x + self.selection_start.x() * self.zoom_level,
                    canvas_y + self.selection_start.y() * self.zoom_level
                )
                if self.last_point:
                    end_screen = QPointF(
                        canvas_x + self.last_point.x() * self.zoom_level,
                        canvas_y + self.last_point.y() * self.zoom_level
                    )
                    rect = QRectF(start_screen, end_screen).normalized()
                    painter.drawRect(rect)

    def _draw_checkerboard(self, painter: QPainter, x: int, y: int, width: int, height: int):
        """Draw checkerboard pattern for transparent background"""
        checker_size = 10
        light_color = QColor(255, 255, 255)
        dark_color = QColor(200, 200, 200)

        for row in range(0, height // checker_size + 1):
            for col in range(0, width // checker_size + 1):
                color = light_color if (row + col) % 2 == 0 else dark_color
                painter.fillRect(x + col * checker_size, y + row * checker_size,
                               checker_size, checker_size, color)

    def _draw_onion_skin(self, painter: QPainter, x: int, y: int, width: int, height: int):
        """Draw onion skin frames (previous/next frames semi-transparently)"""
        # Draw previous frames (in red tint)
        for i in range(1, self.onion_skin_before + 1):
            frame_index = self.current_frame - i
            if frame_index >= 0:
                opacity = 0.3 * (1.0 - (i - 1) * 0.3 / self.onion_skin_before)  # Fade out older frames
                self._draw_frame_at_index(painter, x, y, width, height, frame_index,
                                         QColor(255, 0, 0), opacity)

        # Draw next frames (in blue tint)
        for i in range(1, self.onion_skin_after + 1):
            frame_index = self.current_frame + i
            if frame_index < self.get_frame_count():
                opacity = 0.3 * (1.0 - (i - 1) * 0.3 / self.onion_skin_after)  # Fade out further frames
                self._draw_frame_at_index(painter, x, y, width, height, frame_index,
                                         QColor(0, 0, 255), opacity)

    def _draw_frame_at_index(self, painter: QPainter, x: int, y: int, width: int, height: int,
                            frame_index: int, tint_color: QColor, opacity: float):
        """Draw a specific frame with tint and opacity"""
        # Temporarily set layers to this frame
        old_indices = [layer._current_frame_index for layer in self.layers]

        for layer in self.layers:
            if frame_index < len(layer.frames):
                layer._current_frame_index = frame_index
            else:
                layer._current_frame_index = len(layer.frames) - 1

        # Composite this frame WITHOUT background so it's transparent
        frame_composite = self.composite_layers(include_background=False)

        # Apply tint
        tinted = QPixmap(frame_composite.size())
        tinted.fill(Qt.GlobalColor.transparent)
        tint_painter = QPainter(tinted)

        # Draw the original frame
        tint_painter.drawPixmap(0, 0, frame_composite)

        # Apply tint overlay
        tint_painter.setCompositionMode(QPainter.CompositionMode.CompositionMode_SourceAtop)
        tint_with_alpha = QColor(tint_color)
        tint_with_alpha.setAlpha(100)
        tint_painter.fillRect(tinted.rect(), tint_with_alpha)
        tint_painter.end()

        # Scale if needed
        if self.zoom_level != 1.0:
            tinted = tinted.scaled(width, height,
                                  Qt.AspectRatioMode.KeepAspectRatio,
                                  Qt.TransformationMode.FastTransformation)

        # Draw with opacity
        painter.setOpacity(opacity)
        painter.setCompositionMode(QPainter.CompositionMode.CompositionMode_SourceOver)
        painter.drawPixmap(x, y, tinted)
        painter.setOpacity(1.0)
        painter.setCompositionMode(QPainter.CompositionMode.CompositionMode_SourceOver)

        # Restore layer frame indices
        for layer, old_index in zip(self.layers, old_indices):
            layer._current_frame_index = old_index

    def mousePressEvent(self, event):
        """Handle mouse press for drawing, moving, and selection"""
        if event.button() == Qt.MouseButton.LeftButton:
            canvas_pos = self._screen_to_canvas(event.pos())

            if self.current_tool == "move":
                # Start moving layer
                layer = self.get_current_layer()
                if layer and not layer.locked:
                    self.is_moving_layer = True
                    self.move_start_pos = canvas_pos
                    self.layer_start_offset = (layer.offset_x, layer.offset_y)
                    self.layer_before_drawing = layer.canvas.copy()

            elif self.current_tool == "lasso":
                # Start lasso selection
                if self._is_point_in_canvas(canvas_pos):
                    self.is_selecting = True
                    self.selection_points = [canvas_pos]
                    self.last_point = canvas_pos

            elif self.current_tool == "marquee":
                # Start marquee selection
                if self._is_point_in_canvas(canvas_pos):
                    self.is_selecting = True
                    self.selection_start = canvas_pos
                    self.last_point = canvas_pos

            elif self.current_tool == "fill":
                # Fill bucket tool
                if self._is_point_in_canvas(canvas_pos):
                    layer = self.get_current_layer()
                    if layer and not layer.locked:
                        self.layer_before_drawing = layer.canvas.copy()
                        self._flood_fill(canvas_pos, self.brush_color)
                        # Create undo command
                        cmd = DrawCommand(layer, self.layer_before_drawing)
                        self.command_history.execute(cmd)
                        self.layer_before_drawing = None
                        self.history_changed.emit()
                        self.canvas_modified.emit()

            else:
                # Drawing mode (brush/eraser)
                if self._is_point_in_canvas(canvas_pos):
                    self.is_drawing = True
                    self.last_point = canvas_pos

                    # Start tracking drawing for undo/redo
                    layer = self.get_current_layer()
                    if layer and not layer.locked:
                        self.layer_before_drawing = layer.canvas.copy()

                    self._draw_point(canvas_pos, self.current_pressure)

        elif event.button() == Qt.MouseButton.MiddleButton:
            self.last_point = event.pos()

    def mouseMoveEvent(self, event):
        """Handle mouse move for drawing, moving, and selection"""
        if event.buttons() & Qt.MouseButton.LeftButton:
            canvas_pos = self._screen_to_canvas(event.pos())

            if self.is_moving_layer:
                # Move layer (no stabilization for movement)
                layer = self.get_current_layer()
                if layer and self.move_start_pos:
                    delta_x = int(canvas_pos.x() - self.move_start_pos.x())
                    delta_y = int(canvas_pos.y() - self.move_start_pos.y())
                    layer.offset_x = self.layer_start_offset[0] + delta_x
                    layer.offset_y = self.layer_start_offset[1] + delta_y
                    self.update()

            elif self.is_selecting and self.current_tool == "lasso":
                # Add point to lasso selection (with distance sampling to prevent too many points)
                if self._is_point_in_canvas(canvas_pos):
                    # Only add point if it's far enough from the last point
                    if not self.selection_points or self._distance(canvas_pos, self.selection_points[-1]) > 2.0:
                        self.selection_points.append(canvas_pos)
                    self.last_point = canvas_pos
                    self.update()

            elif self.is_selecting and self.current_tool == "marquee":
                # Update marquee rectangle
                if self._is_point_in_canvas(canvas_pos):
                    self.last_point = canvas_pos
                    self.update()

            elif self.is_drawing:
                # Apply stabilization to drawing
                stabilized_pos = self._apply_stabilization(canvas_pos)
                if self.last_point:
                    self._draw_line(self.last_point, stabilized_pos, self.current_pressure)
                self.last_point = stabilized_pos

        elif event.buttons() & Qt.MouseButton.MiddleButton:
            # Pan
            if self.last_point:
                delta = event.pos() - self.last_point
                self.pan_offset += delta
                self.last_point = event.pos()
                self.update()

    def mouseReleaseEvent(self, event):
        """Handle mouse release"""
        if event.button() == Qt.MouseButton.LeftButton:
            if self.is_moving_layer:
                # End layer move - apply transformation to pixel data
                layer = self.get_current_layer()
                if layer and self.layer_start_offset is not None:
                    # Only create command if layer actually moved
                    if layer.offset_x != 0 or layer.offset_y != 0:
                        before_canvas = self.layer_before_drawing if self.layer_before_drawing else layer.canvas.copy()
                        new_canvas = QPixmap(layer.canvas.width(), layer.canvas.height())
                        new_canvas.fill(Qt.GlobalColor.transparent)
                        painter = QPainter(new_canvas)
                        painter.drawPixmap(layer.offset_x, layer.offset_y, layer.canvas)
                        painter.end()
                        layer.canvas = new_canvas
                        layer.offset_x = 0
                        layer.offset_y = 0
                        cmd = MoveLayerCommand(layer, before_canvas)
                        self.command_history.execute(cmd)
                        self.history_changed.emit()

                self.is_moving_layer = False
                self.move_start_pos = None
                self.layer_start_offset = None
                self.layer_before_drawing = None
                self.canvas_modified.emit()

            elif self.is_selecting:
                # Finalize selection
                if self.current_tool == "lasso" and len(self.selection_points) > 2:
                    # Create lasso selection
                    add_to_existing = self.shift_pressed
                    self.create_lasso_selection(self.selection_points, add_to_existing)
                    self.selection_points.clear()

                elif self.current_tool == "marquee" and self.selection_start and self.last_point:
                    # Create marquee selection
                    x1, y1 = int(self.selection_start.x()), int(self.selection_start.y())
                    x2, y2 = int(self.last_point.x()), int(self.last_point.y())
                    rect = QRect(QPoint(x1, y1), QPoint(x2, y2)).normalized()
                    add_to_existing = self.shift_pressed
                    self.create_rect_selection(rect, add_to_existing)
                    self.selection_start = None

                self.is_selecting = False
                self.last_point = None
                self.update()

            elif self.is_drawing:
                # End drawing
                self.is_drawing = False
                self.last_point = None
                self._clear_stabilization_buffer()

                # Create undo command for drawing
                layer = self.get_current_layer()
                if layer and self.layer_before_drawing is not None:
                    cmd = DrawCommand(layer, self.layer_before_drawing)
                    self.command_history.execute(cmd)
                    self.layer_before_drawing = None
                    self.history_changed.emit()

                self.canvas_modified.emit()

        elif event.button() == Qt.MouseButton.MiddleButton:
            self.last_point = None

    def tabletEvent(self, event: QTabletEvent):
        """Handle tablet events for pen pressure"""
        # Get pressure from tablet
        pressure = event.pressure()

        # Update pressure only if it's valid (above threshold)
        # This prevents the bug where lifting the pen creates a full-size dot
        if pressure > 0.01:
            self.current_pressure = pressure
        elif pressure == 0:
            # Pen is being lifted or not in contact - don't update pressure
            # This maintains the last valid pressure value
            pass

        # Convert to mouse event
        pos = event.position().toPoint()
        canvas_pos = self._screen_to_canvas(pos)

        # Handle non-drawing tools
        if self.current_tool == "move":
            if event.type() == QTabletEvent.Type.TabletPress:
                layer = self.get_current_layer()
                if layer and not layer.locked:
                    self.is_moving_layer = True
                    self.move_start_pos = canvas_pos
                    self.layer_start_offset = (layer.offset_x, layer.offset_y)
                    self.layer_before_drawing = layer.canvas.copy()
            elif event.type() == QTabletEvent.Type.TabletMove:
                if self.is_moving_layer:
                    layer = self.get_current_layer()
                    if layer and self.move_start_pos:
                        delta_x = int(canvas_pos.x() - self.move_start_pos.x())
                        delta_y = int(canvas_pos.y() - self.move_start_pos.y())
                        layer.offset_x = self.layer_start_offset[0] + delta_x
                        layer.offset_y = self.layer_start_offset[1] + delta_y
                        self.update()
            elif event.type() == QTabletEvent.Type.TabletRelease:
                if self.is_moving_layer:
                    layer = self.get_current_layer()
                    if layer and self.layer_start_offset is not None:
                        if layer.offset_x != 0 or layer.offset_y != 0:
                            before_canvas = self.layer_before_drawing if self.layer_before_drawing else layer.canvas.copy()
                            new_canvas = QPixmap(layer.canvas.width(), layer.canvas.height())
                            new_canvas.fill(Qt.GlobalColor.transparent)
                            painter = QPainter(new_canvas)
                            painter.drawPixmap(layer.offset_x, layer.offset_y, layer.canvas)
                            painter.end()
                            layer.canvas = new_canvas
                            layer.offset_x = 0
                            layer.offset_y = 0
                            cmd = MoveLayerCommand(layer, before_canvas)
                            self.command_history.execute(cmd)
                            self.history_changed.emit()
                    self.is_moving_layer = False
                    self.move_start_pos = None
                    self.layer_start_offset = None
                    self.layer_before_drawing = None
                    self.canvas_modified.emit()
            event.accept()
            return

        elif self.current_tool == "lasso":
            if event.type() == QTabletEvent.Type.TabletPress:
                if self._is_point_in_canvas(canvas_pos):
                    self.is_selecting = True
                    self.selection_points = [canvas_pos]
                    self.last_point = canvas_pos
            elif event.type() == QTabletEvent.Type.TabletMove:
                if self.is_selecting and self._is_point_in_canvas(canvas_pos):
                    # Only add point if it's far enough from the last point
                    if not self.selection_points or self._distance(canvas_pos, self.selection_points[-1]) > 2.0:
                        self.selection_points.append(canvas_pos)
                    self.last_point = canvas_pos
                    self.update()
            elif event.type() == QTabletEvent.Type.TabletRelease:
                if self.is_selecting and len(self.selection_points) > 2:
                    add_to_existing = self.shift_pressed
                    self.create_lasso_selection(self.selection_points, add_to_existing)
                    self.selection_points.clear()
                self.is_selecting = False
                self.last_point = None
                self.update()
            event.accept()
            return

        elif self.current_tool == "marquee":
            if event.type() == QTabletEvent.Type.TabletPress:
                if self._is_point_in_canvas(canvas_pos):
                    self.is_selecting = True
                    self.selection_start = canvas_pos
                    self.last_point = canvas_pos
            elif event.type() == QTabletEvent.Type.TabletMove:
                if self.is_selecting and self._is_point_in_canvas(canvas_pos):
                    self.last_point = canvas_pos
                    self.update()
            elif event.type() == QTabletEvent.Type.TabletRelease:
                if self.is_selecting and self.selection_start and self.last_point:
                    x1, y1 = int(self.selection_start.x()), int(self.selection_start.y())
                    x2, y2 = int(self.last_point.x()), int(self.last_point.y())
                    rect = QRect(QPoint(x1, y1), QPoint(x2, y2)).normalized()
                    add_to_existing = self.shift_pressed
                    self.create_rect_selection(rect, add_to_existing)
                    self.selection_start = None
                self.is_selecting = False
                self.last_point = None
                self.update()
            event.accept()
            return

        elif self.current_tool == "fill":
            if event.type() == QTabletEvent.Type.TabletPress:
                if self._is_point_in_canvas(canvas_pos):
                    layer = self.get_current_layer()
                    if layer and not layer.locked:
                        self.layer_before_drawing = layer.canvas.copy()
                        self._flood_fill(canvas_pos, self.brush_color)
                        cmd = DrawCommand(layer, self.layer_before_drawing)
                        self.command_history.execute(cmd)
                        self.layer_before_drawing = None
                        self.history_changed.emit()
                        self.canvas_modified.emit()
            event.accept()
            return

        # Drawing mode (brush/eraser)
        if event.type() == QTabletEvent.Type.TabletPress:
            canvas_pos = self._screen_to_canvas(pos)
            if self._is_point_in_canvas(canvas_pos):
                self.is_drawing = True
                self.last_point = canvas_pos

                # Start tracking drawing for undo/redo
                layer = self.get_current_layer()
                if layer and not layer.locked:
                    self.layer_before_drawing = layer.canvas.copy()

                # Only draw if we have valid pressure
                if pressure > 0.01:
                    self._draw_point(canvas_pos, self.current_pressure)
        elif event.type() == QTabletEvent.Type.TabletMove:
            if self.is_drawing and pressure > 0.01:
                # Only draw if pressure is above threshold
                canvas_pos = self._screen_to_canvas(pos)
                # Apply stabilization to pen drawing
                stabilized_pos = self._apply_stabilization(canvas_pos)
                if self.last_point:
                    self._draw_line(self.last_point, stabilized_pos, self.current_pressure)
                self.last_point = stabilized_pos
        elif event.type() == QTabletEvent.Type.TabletRelease:
            # Don't draw on release - pen is already lifted
            self.is_drawing = False
            self.last_point = None
            self._clear_stabilization_buffer()  # Clear stabilization buffer
            # Reset pressure to default for mouse mode
            self.current_pressure = 1.0

            # Create undo command for drawing
            layer = self.get_current_layer()
            if layer and self.layer_before_drawing is not None:
                cmd = DrawCommand(layer, self.layer_before_drawing)
                self.command_history.execute(cmd)
                self.layer_before_drawing = None
                self.history_changed.emit()

            self.canvas_modified.emit()

        event.accept()

    def keyPressEvent(self, event):
        """Handle key press events"""
        if event.key() == Qt.Key.Key_Shift:
            self.shift_pressed = True
        event.accept()

    def keyReleaseEvent(self, event):
        """Handle key release events"""
        if event.key() == Qt.Key.Key_Shift:
            self.shift_pressed = False
        event.accept()

    def wheelEvent(self, event):
        """Handle mouse wheel for zoom"""
        delta = event.angleDelta().y() / 120.0
        zoom_factor = 1.1 if delta > 0 else 0.9

        # Limit zoom range
        new_zoom = self.zoom_level * zoom_factor
        if 0.1 <= new_zoom <= 20.0:
            self.zoom_level = new_zoom
            self.update()

    def _screen_to_canvas(self, screen_pos: QPoint) -> QPointF:
        """Convert screen coordinates to canvas coordinates"""
        scaled_width = int(self.canvas_width * self.zoom_level)
        scaled_height = int(self.canvas_height * self.zoom_level)

        canvas_x = (self.width() - scaled_width) // 2 + self.pan_offset.x()
        canvas_y = (self.height() - scaled_height) // 2 + self.pan_offset.y()

        # Convert to canvas space
        x = (screen_pos.x() - canvas_x) / self.zoom_level
        y = (screen_pos.y() - canvas_y) / self.zoom_level

        return QPointF(x, y)

    def _is_point_in_canvas(self, canvas_pos: QPointF) -> bool:
        """Check if point is within canvas bounds"""
        return (0 <= canvas_pos.x() < self.canvas_width and
                0 <= canvas_pos.y() < self.canvas_height)

    def _distance(self, p1: QPointF, p2: QPointF) -> float:
        """Calculate distance between two points"""
        dx = p1.x() - p2.x()
        dy = p1.y() - p2.y()
        return math.sqrt(dx * dx + dy * dy)

    def _draw_point(self, pos: QPointF, pressure: float):
        """Draw a single point (respects selection)"""
        layer = self.get_current_layer()
        if not layer or layer.locked:
            return

        # If there's a selection and point is outside, don't draw
        if not self.is_point_in_selection(pos):
            return

        painter = QPainter(layer.canvas)
        self._setup_painter(painter, pressure)

        # Draw point as small circle
        actual_size = self._get_pressure_adjusted_size(pressure)

        # If there's a selection mask, use it as a clip region
        if self.selection_mask is not None:
            # Create alpha mask - white (opaque) areas in selection_mask become the clip region
            mask_image = self.selection_mask.createAlphaMask()
            mask_bitmap = QBitmap.fromImage(mask_image)
            painter.setClipRegion(QRegion(mask_bitmap))

        painter.drawEllipse(pos, actual_size / 2, actual_size / 2)

        painter.end()
        self.update()

    def _draw_line(self, start: QPointF, end: QPointF, pressure: float):
        """Draw a line with interpolation (respects selection)"""
        layer = self.get_current_layer()
        if not layer or layer.locked:
            return

        painter = QPainter(layer.canvas)
        self._setup_painter(painter, pressure)

        # If there's a selection mask, use it as a clip region
        if self.selection_mask is not None:
            # Create alpha mask - white (opaque) areas in selection_mask become the clip region
            mask_image = self.selection_mask.createAlphaMask()
            mask_bitmap = QBitmap.fromImage(mask_image)
            painter.setClipRegion(QRegion(mask_bitmap))

        # Calculate distance for interpolation
        distance = math.sqrt((end.x() - start.x())**2 + (end.y() - start.y())**2)
        steps = max(int(distance), 1)

        # Interpolate points for smooth line
        for i in range(steps + 1):
            t = i / steps if steps > 0 else 0
            x = start.x() + (end.x() - start.x()) * t
            y = start.y() + (end.y() - start.y()) * t

            point = QPointF(x, y)
            # Only draw if within selection (checked by clip region)
            actual_size = self._get_pressure_adjusted_size(pressure)
            painter.drawEllipse(point, actual_size / 2, actual_size / 2)

        painter.end()
        self.update()

    def _setup_painter(self, painter: QPainter, pressure: float):
        """Setup painter for drawing"""
        if self.brush_antialias:
            painter.setRenderHint(QPainter.RenderHint.Antialiasing)

        actual_opacity = self._get_pressure_adjusted_opacity(pressure)
        painter.setOpacity(actual_opacity)

        if self.current_tool == "eraser":
            painter.setCompositionMode(QPainter.CompositionMode.CompositionMode_Clear)
            painter.setPen(Qt.PenStyle.NoPen)
            painter.setBrush(QBrush(QColor(0, 0, 0, 255)))
        else:  # brush
            painter.setCompositionMode(QPainter.CompositionMode.CompositionMode_SourceOver)
            painter.setPen(Qt.PenStyle.NoPen)
            painter.setBrush(QBrush(self.brush_color))

    def _get_pressure_adjusted_size(self, pressure: float) -> float:
        """Get brush size adjusted for pressure"""
        if self.use_pressure_size:
            return self.brush_size * pressure
        return self.brush_size

    def _get_pressure_adjusted_opacity(self, pressure: float) -> float:
        """Get opacity adjusted for pressure"""
        if self.use_pressure_opacity:
            return self.brush_opacity * pressure
        return self.brush_opacity

    def _apply_stabilization(self, point: QPointF) -> QPointF:
        """Apply stabilization/smoothing to input point"""
        if self.stabilization_amount == 0:
            return point

        # Add point to buffer
        self.stabilization_buffer.append(point)

        # Calculate buffer size based on stabilization amount (1-10 points)
        max_buffer_size = max(1, int(self.stabilization_amount / 10))

        # Limit buffer size
        if len(self.stabilization_buffer) > max_buffer_size:
            self.stabilization_buffer.pop(0)

        # Calculate weighted average (more recent points have more weight)
        if len(self.stabilization_buffer) == 1:
            return point

        total_weight = 0.0
        avg_x = 0.0
        avg_y = 0.0

        for i, p in enumerate(self.stabilization_buffer):
            # Weight increases linearly from oldest to newest
            weight = (i + 1) / len(self.stabilization_buffer)
            avg_x += p.x() * weight
            avg_y += p.y() * weight
            total_weight += weight

        return QPointF(avg_x / total_weight, avg_y / total_weight)

    def _clear_stabilization_buffer(self):
        """Clear the stabilization buffer"""
        self.stabilization_buffer.clear()

    # ========================================================================
    # Selection Operations
    # ========================================================================

    def has_selection(self) -> bool:
        """Check if there's an active selection"""
        return self.selection_mask is not None

    def clear_selection(self):
        """Clear the current selection"""
        self.selection_mask = None
        self.update()

    def create_rect_selection(self, rect: QRect, add_to_existing: bool = False):
        """Create a rectangular selection"""
        if self.selection_mask is None or not add_to_existing:
            self.selection_mask = QImage(self.canvas_width, self.canvas_height, QImage.Format.Format_ARGB32)
            self.selection_mask.fill(Qt.GlobalColor.transparent)

        painter = QPainter(self.selection_mask)
        painter.setCompositionMode(QPainter.CompositionMode.CompositionMode_Source)
        painter.fillRect(rect, QColor(255, 255, 255, 255))
        painter.end()
        self.update()

    def create_lasso_selection(self, points: List[QPointF], add_to_existing: bool = False):
        """Create a lasso (freeform) selection from a list of points"""
        if not points or len(points) < 3:
            return

        # For large canvases with many points, simplify the path
        if len(points) > 1000:
            # Downsample to every Nth point to improve performance
            step = len(points) // 500
            points = points[::step]

        if self.selection_mask is None or not add_to_existing:
            self.selection_mask = QImage(self.canvas_width, self.canvas_height, QImage.Format.Format_ARGB32)
            self.selection_mask.fill(Qt.GlobalColor.transparent)

        painter = QPainter(self.selection_mask)
        painter.setCompositionMode(QPainter.CompositionMode.CompositionMode_Source)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)

        # Create path from points
        path = QPainterPath()
        path.moveTo(points[0])
        for point in points[1:]:
            path.lineTo(point)
        path.closeSubpath()

        painter.fillPath(path, QColor(255, 255, 255, 255))
        painter.end()
        self.update()

    def is_point_in_selection(self, point: QPointF) -> bool:
        """Check if a point is within the selection"""
        if self.selection_mask is None:
            return True  # No selection = draw everywhere

        x = int(point.x())
        y = int(point.y())

        if x < 0 or x >= self.canvas_width or y < 0 or y >= self.canvas_height:
            return False

        # Check if pixel is selected (white/non-transparent in mask)
        pixel = self.selection_mask.pixelColor(x, y)
        return pixel.alpha() > 128

    def _flood_fill(self, start_point: QPointF, fill_color: QColor):
        """Flood fill algorithm with tolerance and gap closing that respects selection"""
        layer = self.get_current_layer()
        if not layer or layer.locked:
            return

        x = int(start_point.x())
        y = int(start_point.y())

        if x < 0 or x >= self.canvas_width or y < 0 or y >= self.canvas_height:
            return

        # Get the target color to replace
        image = layer.canvas.toImage()
        target_color = QColor(image.pixelColor(x, y))

        # Don't fill if target is same as fill color (with tolerance)
        if self._color_matches(target_color, fill_color, 0):
            return

        # Apply gap closing preprocessing if enabled
        if self.fill_close_gaps > 0:
            image = self._close_gaps(image, self.fill_close_gaps)

        # Use a stack-based flood fill to avoid recursion limits
        stack = [(x, y)]
        visited = set()

        while stack:
            cx, cy = stack.pop()

            # Skip if out of bounds
            if cx < 0 or cx >= self.canvas_width or cy < 0 or cy >= self.canvas_height:
                continue

            # Skip if already visited
            if (cx, cy) in visited:
                continue

            # Skip if not in selection
            if not self.is_point_in_selection(QPointF(cx, cy)):
                continue

            # Check if this pixel matches target color with tolerance
            current_color = QColor(image.pixelColor(cx, cy))
            if not self._color_matches(current_color, target_color, self.fill_tolerance):
                continue

            # Mark as visited and fill
            visited.add((cx, cy))
            image.setPixelColor(cx, cy, fill_color)

            # Add neighbors to stack
            stack.append((cx + 1, cy))
            stack.append((cx - 1, cy))
            stack.append((cx, cy + 1))
            stack.append((cx, cy - 1))

        # Update layer with filled image
        layer.canvas = QPixmap.fromImage(image)
        self.update()

    def _color_matches(self, color1: QColor, color2: QColor, tolerance: int) -> bool:
        """Check if two colors match within tolerance"""
        if tolerance == 0:
            return color1.rgba() == color2.rgba()

        # Calculate color distance using Euclidean distance in RGB space
        r_diff = abs(color1.red() - color2.red())
        g_diff = abs(color1.green() - color2.green())
        b_diff = abs(color1.blue() - color2.blue())
        a_diff = abs(color1.alpha() - color2.alpha())

        # Use maximum component difference (simpler and faster than Euclidean)
        max_diff = max(r_diff, g_diff, b_diff, a_diff)
        return max_diff <= tolerance

    def _close_gaps(self, image: QImage, gap_size: int) -> QImage:
        """Close small gaps in the image by dilating dark pixels - optimized version"""
        # For large canvases, limit gap size to prevent performance issues
        if self.canvas_width * self.canvas_height > 500000:  # > 500k pixels
            gap_size = min(gap_size, 2)  # Limit to 2 pixels max for large canvases

        # Create a copy to work with
        result = image.copy()

        # Collect dark pixels first to avoid redundant processing
        dark_pixels = []
        for y in range(0, self.canvas_height, max(1, gap_size // 2)):  # Sample, don't check every pixel
            for x in range(0, self.canvas_width, max(1, gap_size // 2)):
                pixel = QColor(image.pixelColor(x, y))
                luminance = (pixel.red() * 0.299 + pixel.green() * 0.587 + pixel.blue() * 0.114)
                if luminance < 128:  # Dark pixel
                    dark_pixels.append((x, y, pixel))

        # Dilate dark pixels
        for x, y, pixel in dark_pixels:
            # Dilate by setting nearby pixels to this color
            for dy in range(-gap_size, gap_size + 1):
                for dx in range(-gap_size, gap_size + 1):
                    nx, ny = x + dx, y + dy
                    if 0 <= nx < self.canvas_width and 0 <= ny < self.canvas_height:
                        # Only fill transparent or very light pixels
                        neighbor = QColor(result.pixelColor(nx, ny))
                        neighbor_lum = (neighbor.red() * 0.299 + neighbor.green() * 0.587 + neighbor.blue() * 0.114)
                        if neighbor.alpha() < 128 or neighbor_lum > 200:
                            result.setPixelColor(nx, ny, pixel)

        return result

    def reset_view(self):
        """Reset zoom and pan"""
        self.zoom_level = 1.0
        self.pan_offset = QPoint(0, 0)
        self.update()

    def save_to_file(self, file_path: str) -> bool:
        """Save canvas to image file"""
        composite = self.composite_layers()
        return composite.save(file_path)

    def export_layer(self, layer_index: int, file_path: str) -> bool:
        """Export a specific layer"""
        if 0 <= layer_index < len(self.layers):
            return self.layers[layer_index].canvas.save(file_path)
        return False

    # ========================================================================
    # Undo/Redo Operations
    # ========================================================================

    def undo(self):
        """Undo last operation"""
        if self.command_history.undo():
            self.history_changed.emit()
            self.update()
            return True
        return False

    def redo(self):
        """Redo last undone operation"""
        if self.command_history.redo():
            self.history_changed.emit()
            self.update()
            return True
        return False

    def can_undo(self) -> bool:
        """Check if undo is available"""
        return self.command_history.can_undo()

    def can_redo(self) -> bool:
        """Check if redo is available"""
        return self.command_history.can_redo()

    # ========================================================================
    # Clipboard Operations
    # ========================================================================

    def copy_layer(self) -> Optional[Layer]:
        """Copy current layer to clipboard"""
        layer = self.get_current_layer()
        if layer:
            return layer.copy()
        return None

    def cut_layer(self) -> Optional[Layer]:
        """Cut current layer"""
        if len(self.layers) <= 1:
            return None  # Can't cut the last layer

        layer = self.get_current_layer()
        if layer:
            # Copy layer
            layer_copy = layer.copy()

            # Remove layer with command
            index = self.current_layer_index
            cmd = RemoveLayerCommand(self, layer, index)
            self.command_history.execute(cmd)
            self.history_changed.emit()
            self.update()

            return layer_copy
        return None

    def paste_layer(self, layer: Layer):
        """Paste a layer"""
        if layer:
            # Resize to match canvas if needed
            if layer.canvas.width() != self.canvas_width or layer.canvas.height() != self.canvas_height:
                layer.resize(self.canvas_width, self.canvas_height)

            cmd = PasteLayerCommand(self, layer)
            self.command_history.execute(cmd)
            self.history_changed.emit()
            self.update()

    def paste_image_from_clipboard(self, image: QImage):
        """Paste an image from clipboard as new layer"""
        if image.isNull():
            return False

        # Create new layer from image
        layer = Layer(f"Pasted Image", self.canvas_width, self.canvas_height)

        # Draw image onto layer
        painter = QPainter(layer.canvas)

        # Scale image to fit canvas if larger
        if image.width() > self.canvas_width or image.height() > self.canvas_height:
            image = image.scaled(self.canvas_width, self.canvas_height,
                               Qt.AspectRatioMode.KeepAspectRatio,
                               Qt.TransformationMode.SmoothTransformation)

        # Center image on canvas
        x = (self.canvas_width - image.width()) // 2
        y = (self.canvas_height - image.height()) // 2

        painter.drawImage(x, y, image)
        painter.end()

        # Paste layer
        cmd = PasteLayerCommand(self, layer)
        self.command_history.execute(cmd)
        self.history_changed.emit()
        self.update()

        return True


class ColorWheelWidget(QWidget):
    """Color wheel widget for color selection"""

    color_changed = pyqtSignal(QColor)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.current_color = QColor(0, 0, 0)
        self.setMinimumSize(200, 200)
        self.setMaximumSize(300, 300)

    def paintEvent(self, event):
        """Draw color wheel"""
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)

        # Draw simple color gradient (simplified color wheel)
        center_x = self.width() // 2
        center_y = self.height() // 2
        radius = min(center_x, center_y) - 10

        # Draw color wheel using HSV
        for angle in range(0, 360, 5):
            for r in range(0, radius, 5):
                color = QColor.fromHsv(angle, int(255 * r / radius), 255)
                painter.setPen(color)
                painter.setBrush(color)

                x = center_x + int(r * math.cos(math.radians(angle)))
                y = center_y + int(r * math.sin(math.radians(angle)))
                painter.drawEllipse(x - 2, y - 2, 4, 4)

        # Draw current color indicator with theme border
        theme_manager = get_theme_manager()
        theme = theme_manager.get_current_theme()
        painter.setPen(QPen(QColor(theme.colors.border_focus), 2))
        painter.setBrush(self.current_color)
        painter.drawEllipse(center_x - 15, center_y - 15, 30, 30)

    def mousePressEvent(self, event):
        """Handle color selection"""
        self._pick_color_at(event.pos())

    def mouseMoveEvent(self, event):
        """Handle color dragging"""
        if event.buttons() & Qt.MouseButton.LeftButton:
            self._pick_color_at(event.pos())

    def _pick_color_at(self, pos: QPoint):
        """Pick color at position"""
        center_x = self.width() // 2
        center_y = self.height() // 2

        dx = pos.x() - center_x
        dy = pos.y() - center_y

        angle = math.degrees(math.atan2(dy, dx))
        if angle < 0:
            angle += 360

        distance = math.sqrt(dx * dx + dy * dy)
        radius = min(center_x, center_y) - 10
        saturation = min(int(255 * distance / radius), 255)

        self.current_color = QColor.fromHsv(int(angle), saturation, 255)
        self.color_changed.emit(self.current_color)
        self.update()

    def set_color(self, color: QColor):
        """Set current color"""
        self.current_color = color
        self.update()


class NewCanvasDialog(QDialog):
    """Dialog for creating a new canvas"""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("New Canvas")
        self.setModal(True)
        self.setMinimumWidth(350)

        layout = QVBoxLayout()

        form_layout = QFormLayout()

        # Width
        self.width_spin = QSpinBox()
        self.width_spin.setMinimum(1)
        self.width_spin.setMaximum(4096)
        self.width_spin.setValue(100)
        self.width_spin.setSuffix(" px")
        form_layout.addRow("Width:", self.width_spin)

        # Height
        self.height_spin = QSpinBox()
        self.height_spin.setMinimum(1)
        self.height_spin.setMaximum(4096)
        self.height_spin.setValue(100)
        self.height_spin.setSuffix(" px")
        form_layout.addRow("Height:", self.height_spin)

        # Background color
        bg_layout = QHBoxLayout()
        self.bg_enabled = QCheckBox("Use background color")
        self.bg_enabled.setChecked(True)
        self.bg_enabled.toggled.connect(self._on_bg_toggled)
        bg_layout.addWidget(self.bg_enabled)

        self.bg_color_btn = QPushButton()
        self.bg_color_btn.setFixedSize(60, 30)
        self.bg_color = QColor(200, 200, 200)
        self._update_bg_color_button()
        self.bg_color_btn.clicked.connect(self._pick_bg_color)
        bg_layout.addWidget(self.bg_color_btn)
        bg_layout.addStretch()

        form_layout.addRow("Background:", bg_layout)

        layout.addLayout(form_layout)

        # Buttons
        buttons = QDialogButtonBox(
            QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel
        )
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)

        self.setLayout(layout)

    def _on_bg_toggled(self, checked: bool):
        """Handle background toggle"""
        self.bg_color_btn.setEnabled(checked)

    def _pick_bg_color(self):
        """Pick background color"""
        color = QColorDialog.getColor(self.bg_color, self, "Select Background Color")
        if color.isValid():
            self.bg_color = color
            self._update_bg_color_button()

    def _update_bg_color_button(self):
        """Update background color button"""
        self.bg_color_btn.setStyleSheet(
            f"background-color: rgb({self.bg_color.red()}, {self.bg_color.green()}, {self.bg_color.blue()});"
        )

    def get_canvas_data(self):
        """Get canvas data"""
        return {
            'width': self.width_spin.value(),
            'height': self.height_spin.value(),
            'bg_enabled': self.bg_enabled.isChecked(),
            'bg_color': self.bg_color
        }


class SpriteSheetExportDialog(QDialog):
    """Dialog for exporting animation as sprite sheet"""

    def __init__(self, canvas: CanvasWidget, parent=None):
        super().__init__(parent)
        self.canvas = canvas
        self.setWindowTitle("Export Sprite Sheet")
        self.setMinimumWidth(400)

        # Get theme
        theme_manager = get_theme_manager()
        theme = theme_manager.get_current_theme()
        self.setStyleSheet(f"background-color: {theme.colors.surface};")

        layout = QVBoxLayout()

        # Layout mode
        layout_group = QGroupBox("Layout")
        layout_form = QFormLayout()

        self.layout_mode = QComboBox()
        self.layout_mode.addItems(["Fixed Columns", "Fixed Rows", "Single Row", "One Row Per Tag"])
        self.layout_mode.currentIndexChanged.connect(self._on_layout_changed)
        layout_form.addRow("Layout Mode:", self.layout_mode)

        self.columns_spin = QSpinBox()
        self.columns_spin.setRange(1, 100)
        self.columns_spin.setValue(8)
        layout_form.addRow("Columns:", self.columns_spin)

        self.rows_spin = QSpinBox()
        self.rows_spin.setRange(1, 100)
        self.rows_spin.setValue(1)
        self.rows_spin.setEnabled(False)
        layout_form.addRow("Rows:", self.rows_spin)

        layout_group.setLayout(layout_form)
        layout.addWidget(layout_group)

        # Options
        options_group = QGroupBox("Options")
        options_layout = QVBoxLayout()

        self.skip_duplicates = QCheckBox("Skip Duplicate Frames")
        self.skip_duplicates.setChecked(False)
        options_layout.addWidget(self.skip_duplicates)

        self.export_json = QCheckBox("Export JSON Metadata")
        self.export_json.setChecked(True)
        options_layout.addWidget(self.export_json)

        options_group.setLayout(options_layout)
        layout.addWidget(options_group)

        # Buttons
        buttons = QDialogButtonBox(QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel)
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)

        self.setLayout(layout)

    def _on_layout_changed(self, index: int):
        """Handle layout mode change"""
        mode = self.layout_mode.currentText()
        if mode == "Fixed Columns":
            self.columns_spin.setEnabled(True)
            self.rows_spin.setEnabled(False)
        elif mode == "Fixed Rows":
            self.columns_spin.setEnabled(False)
            self.rows_spin.setEnabled(True)
        elif mode == "Single Row":
            self.columns_spin.setEnabled(False)
            self.rows_spin.setEnabled(False)
        elif mode == "One Row Per Tag":
            self.columns_spin.setEnabled(False)
            self.rows_spin.setEnabled(False)

    def get_settings(self):
        """Get export settings"""
        return {
            'layout_mode': self.layout_mode.currentText(),
            'columns': self.columns_spin.value(),
            'rows': self.rows_spin.value(),
            'skip_duplicates': self.skip_duplicates.isChecked(),
            'export_json': self.export_json.isChecked()
        }


class AnimationPreviewWidget(QWidget):
    """Widget for previewing animations with playback controls"""

    def __init__(self, canvas_widget, parent=None):
        super().__init__(parent)
        self.canvas_widget = canvas_widget
        self.current_preview_frame = 0
        self.is_playing = False
        self.playback_timer = QTimer()
        self.playback_timer.timeout.connect(self._advance_frame)
        self.selected_tag = None  # None = all frames

        self.setWindowTitle("Animation Preview")
        self.setMinimumSize(400, 500)

        # Get theme
        theme_manager = get_theme_manager()
        theme = theme_manager.get_current_theme()
        c = theme.colors
        self.setStyleSheet(f"background-color: {c.surface};")

        layout = QVBoxLayout()

        # Preview area (using QLabel to display pixmap)
        self.preview_label = QLabel()
        self.preview_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.preview_label.setStyleSheet(f"background-color: {c.surface}; border: 2px solid {c.border};")
        self.preview_label.setMinimumSize(300, 300)
        layout.addWidget(self.preview_label, 1)

        # Playback controls
        controls = QHBoxLayout()

        self.play_btn = QPushButton("Play")
        self.play_btn.clicked.connect(self._toggle_playback)
        controls.addWidget(self.play_btn)

        self.stop_btn = QPushButton("Stop")
        self.stop_btn.clicked.connect(self._stop_playback)
        controls.addWidget(self.stop_btn)

        self.step_back_btn = QPushButton("< Prev")
        self.step_back_btn.clicked.connect(self._step_back)
        controls.addWidget(self.step_back_btn)

        self.step_forward_btn = QPushButton("Next >")
        self.step_forward_btn.clicked.connect(self._step_forward)
        controls.addWidget(self.step_forward_btn)

        controls.addStretch()

        # FPS control
        controls.addWidget(QLabel("FPS:"))
        self.fps_spin = QSpinBox()
        self.fps_spin.setRange(1, 60)
        self.fps_spin.setValue(12)
        self.fps_spin.valueChanged.connect(self._update_playback_speed)
        controls.addWidget(self.fps_spin)

        layout.addLayout(controls)

        # Animation tag selector
        tag_layout = QHBoxLayout()
        tag_layout.addWidget(QLabel("Animation:"))
        self.tag_combo = QComboBox()
        self.tag_combo.addItem("All Frames", None)
        self.tag_combo.currentIndexChanged.connect(self._on_tag_changed)
        tag_layout.addWidget(self.tag_combo, 1)
        layout.addLayout(tag_layout)

        # Frame counter
        self.frame_info = QLabel("Frame: 1/1")
        self.frame_info.setStyleSheet(f"color: {c.text_secondary};")
        layout.addWidget(self.frame_info)

        self.setLayout(layout)

        # Update tag list and preview
        self._update_tag_list()
        self._update_preview()

    def _update_tag_list(self):
        """Update animation tag dropdown"""
        current_text = self.tag_combo.currentText()
        self.tag_combo.clear()
        self.tag_combo.addItem("All Frames", None)

        for tag in self.canvas_widget.animation_tags:
            self.tag_combo.addItem(tag.name, tag)

        # Try to restore selection
        index = self.tag_combo.findText(current_text)
        if index >= 0:
            self.tag_combo.setCurrentIndex(index)

    def _on_tag_changed(self, index: int):
        """Handle animation tag selection"""
        self.selected_tag = self.tag_combo.itemData(index)
        self._reset_preview()

    def _get_frame_range(self):
        """Get the frame range to play"""
        if self.selected_tag:
            return (self.selected_tag.start_frame, self.selected_tag.end_frame)
        else:
            return (0, self.canvas_widget.get_frame_count() - 1)

    def _reset_preview(self):
        """Reset preview to first frame"""
        start_frame, _ = self._get_frame_range()
        self.current_preview_frame = start_frame
        self._update_preview()

    def _toggle_playback(self):
        """Toggle play/pause"""
        self.is_playing = not self.is_playing
        if self.is_playing:
            self.play_btn.setText("Pause")
            fps = self.fps_spin.value()
            self.playback_timer.start(1000 // fps)
        else:
            self.play_btn.setText("Play")
            self.playback_timer.stop()

    def _stop_playback(self):
        """Stop playback and reset to first frame"""
        self.is_playing = False
        self.play_btn.setText("Play")
        self.playback_timer.stop()
        self._reset_preview()

    def _update_playback_speed(self):
        """Update playback speed when FPS changes"""
        if self.is_playing:
            fps = self.fps_spin.value()
            self.playback_timer.setInterval(1000 // fps)

    def _advance_frame(self):
        """Advance to next frame during playback"""
        start_frame, end_frame = self._get_frame_range()
        self.current_preview_frame += 1
        if self.current_preview_frame > end_frame:
            self.current_preview_frame = start_frame
        self._update_preview()

    def _step_forward(self):
        """Step forward one frame"""
        start_frame, end_frame = self._get_frame_range()
        self.current_preview_frame += 1
        if self.current_preview_frame > end_frame:
            self.current_preview_frame = start_frame
        self._update_preview()

    def _step_back(self):
        """Step back one frame"""
        start_frame, end_frame = self._get_frame_range()
        self.current_preview_frame -= 1
        if self.current_preview_frame < start_frame:
            self.current_preview_frame = end_frame
        self._update_preview()

    def _update_preview(self):
        """Update the preview display"""
        start_frame, end_frame = self._get_frame_range()
        total_frames = end_frame - start_frame + 1
        display_num = (self.current_preview_frame - start_frame) + 1
        self.frame_info.setText(f"Frame: {display_num}/{total_frames}")

        # Render the current frame
        # Temporarily set canvas to preview frame
        old_indices = [layer._current_frame_index for layer in self.canvas_widget.layers]

        for layer in self.canvas_widget.layers:
            if self.current_preview_frame < len(layer.frames):
                layer._current_frame_index = self.current_preview_frame
            else:
                layer._current_frame_index = len(layer.frames) - 1

        # Get composited frame
        composite = self.canvas_widget.composite_layers()

        # Scale to fit in preview label while maintaining aspect ratio
        label_size = self.preview_label.size()
        scaled = composite.scaled(
            label_size.width() - 20,  # Account for border/padding
            label_size.height() - 20,
            Qt.AspectRatioMode.KeepAspectRatio,
            Qt.TransformationMode.FastTransformation
        )

        # Set the pixmap
        self.preview_label.setPixmap(scaled)

        # Restore layer frame indices
        for layer, old_index in zip(self.canvas_widget.layers, old_indices):
            layer._current_frame_index = old_index


class AnimationTagManagerDialog(QDialog):
    """Dialog for managing animation tags"""

    def __init__(self, canvas_widget, parent=None):
        super().__init__(parent)
        self.canvas_widget = canvas_widget

        # Get theme
        theme_manager = get_theme_manager()
        theme = theme_manager.get_current_theme()
        c = theme.colors

        self.setWindowTitle("Animation Tags")
        self.setMinimumSize(500, 400)

        layout = QVBoxLayout()

        # Tag list
        self.tag_list = QListWidget()
        self.tag_list.itemSelectionChanged.connect(self._on_tag_selected)
        layout.addWidget(QLabel("Animation Tags:"))
        layout.addWidget(self.tag_list, 1)

        # Tag controls
        tag_controls = QHBoxLayout()

        self.new_tag_btn = QPushButton("New Tag")
        self.new_tag_btn.clicked.connect(self._new_tag)
        tag_controls.addWidget(self.new_tag_btn)

        self.edit_tag_btn = QPushButton("Edit")
        self.edit_tag_btn.clicked.connect(self._edit_tag)
        self.edit_tag_btn.setEnabled(False)
        tag_controls.addWidget(self.edit_tag_btn)

        self.delete_tag_btn = QPushButton("Delete")
        self.delete_tag_btn.clicked.connect(self._delete_tag)
        self.delete_tag_btn.setEnabled(False)
        tag_controls.addWidget(self.delete_tag_btn)

        tag_controls.addStretch()
        layout.addLayout(tag_controls)

        # Tag properties
        props_group = QGroupBox("Tag Properties")
        props_layout = QFormLayout()

        self.tag_name_edit = QLineEdit()
        props_layout.addRow("Name:", self.tag_name_edit)

        self.start_frame_spin = QSpinBox()
        self.start_frame_spin.setMinimum(0)
        self.start_frame_spin.setMaximum(self.canvas_widget.get_frame_count() - 1)
        props_layout.addRow("Start Frame:", self.start_frame_spin)

        self.end_frame_spin = QSpinBox()
        self.end_frame_spin.setMinimum(0)
        self.end_frame_spin.setMaximum(self.canvas_widget.get_frame_count() - 1)
        props_layout.addRow("End Frame:", self.end_frame_spin)

        self.direction_combo = QComboBox()
        self.direction_combo.addItems(["forward", "reverse", "pingpong"])
        props_layout.addRow("Direction:", self.direction_combo)

        self.apply_btn = QPushButton("Apply Changes")
        self.apply_btn.clicked.connect(self._apply_changes)
        self.apply_btn.setEnabled(False)
        props_layout.addRow("", self.apply_btn)

        props_group.setLayout(props_layout)
        layout.addWidget(props_group)

        # Dialog buttons
        buttons = QDialogButtonBox(QDialogButtonBox.StandardButton.Close)
        buttons.rejected.connect(self.accept)
        layout.addWidget(buttons)

        self.setLayout(layout)

        self._update_tag_list()

    def _update_tag_list(self):
        """Update the tag list display"""
        self.tag_list.clear()
        for tag in self.canvas_widget.animation_tags:
            item_text = f"{tag.name} [{tag.start_frame}-{tag.end_frame}] ({tag.direction})"
            item = QListWidgetItem(item_text)
            item.setData(Qt.ItemDataRole.UserRole, tag)
            self.tag_list.addItem(item)

    def _on_tag_selected(self):
        """Handle tag selection"""
        has_selection = len(self.tag_list.selectedItems()) > 0
        self.edit_tag_btn.setEnabled(has_selection)
        self.delete_tag_btn.setEnabled(has_selection)
        self.apply_btn.setEnabled(has_selection)

        if has_selection:
            tag = self.tag_list.selectedItems()[0].data(Qt.ItemDataRole.UserRole)
            self.tag_name_edit.setText(tag.name)
            self.start_frame_spin.setValue(tag.start_frame)
            self.end_frame_spin.setValue(tag.end_frame)
            self.direction_combo.setCurrentText(tag.direction)

    def _new_tag(self):
        """Create a new tag"""
        tag = AnimationTag("New Animation", 0, self.canvas_widget.get_frame_count() - 1)
        tag.direction = "forward"
        self.canvas_widget.animation_tags.append(tag)
        self._update_tag_list()

        # Select the new tag
        self.tag_list.setCurrentRow(self.tag_list.count() - 1)

    def _edit_tag(self):
        """Edit selected tag"""
        if not self.tag_list.selectedItems():
            return

        # Editing is done through the properties panel
        self.tag_name_edit.setFocus()

    def _delete_tag(self):
        """Delete selected tag"""
        if not self.tag_list.selectedItems():
            return

        tag = self.tag_list.selectedItems()[0].data(Qt.ItemDataRole.UserRole)
        self.canvas_widget.animation_tags.remove(tag)
        self._update_tag_list()

    def _apply_changes(self):
        """Apply changes to selected tag"""
        if not self.tag_list.selectedItems():
            return

        tag = self.tag_list.selectedItems()[0].data(Qt.ItemDataRole.UserRole)
        tag.name = self.tag_name_edit.text()
        tag.start_frame = self.start_frame_spin.value()
        tag.end_frame = self.end_frame_spin.value()
        tag.direction = self.direction_combo.currentText()

        self._update_tag_list()

        QMessageBox.information(self, "Success", "Tag updated successfully")


class ScribblerTool(EditorPanel):
    """Scribbler art tool for creating visual assets"""

    def __init__(self, parent=None, project_root=None):
        self.project_root = project_root
        self.current_file = None
        self.clipboard_layer = None  # For copy/paste operations

        super().__init__("Scribbler", parent)
        self.setObjectName("ScribblerTool")

        # Set focus policy to accept keyboard input
        self.setFocusPolicy(Qt.FocusPolicy.StrongFocus)

        # Create default canvas
        self._create_default_canvas()

        # Setup keyboard shortcuts after UI is created
        QTimer.singleShot(0, self._setup_shortcuts)

        # Ensure focus on show
        self.setFocus()

        # Install event filter to catch key events on child widgets
        self.installEventFilter(self)

    def showEvent(self, event):
        """Override showEvent to ensure focus and shortcuts work"""
        super().showEvent(event)
        self.setFocus()
        self.activateWindow()

    def keyPressEvent(self, event):
        """Handle key press events as fallback for shortcuts"""
        # Handle shortcuts directly as fallback
        modifiers = event.modifiers()
        key = event.key()

        # Ctrl+Z - Undo
        if modifiers == Qt.KeyboardModifier.ControlModifier and key == Qt.Key.Key_Z:
            self._undo()
            event.accept()
            return

        # Ctrl+Shift+Z - Redo
        if modifiers == (Qt.KeyboardModifier.ControlModifier | Qt.KeyboardModifier.ShiftModifier) and key == Qt.Key.Key_Z:
            self._redo()
            event.accept()
            return

        # Ctrl+Y - Redo
        if modifiers == Qt.KeyboardModifier.ControlModifier and key == Qt.Key.Key_Y:
            self._redo()
            event.accept()
            return

        # Ctrl+C - Copy
        if modifiers == Qt.KeyboardModifier.ControlModifier and key == Qt.Key.Key_C:
            self._copy_layer()
            event.accept()
            return

        # Ctrl+X - Cut
        if modifiers == Qt.KeyboardModifier.ControlModifier and key == Qt.Key.Key_X:
            self._cut_layer()
            event.accept()
            return

        # Ctrl+V - Paste
        if modifiers == Qt.KeyboardModifier.ControlModifier and key == Qt.Key.Key_V:
            self._paste()
            event.accept()
            return

        # Ctrl+D - Clear Selection
        if modifiers == Qt.KeyboardModifier.ControlModifier and key == Qt.Key.Key_D:
            self._clear_selection()
            event.accept()
            return

        # Pass event to base class
        super().keyPressEvent(event)

    def eventFilter(self, obj, event):
        """Filter events from child widgets to catch key presses"""
        if event.type() == event.Type.KeyPress:
            # Try to handle as shortcut
            modifiers = event.modifiers()
            key = event.key()

            # Ctrl+Z - Undo
            if modifiers == Qt.KeyboardModifier.ControlModifier and key == Qt.Key.Key_Z:
                self._undo()
                return True

            # Ctrl+Shift+Z - Redo
            if modifiers == (Qt.KeyboardModifier.ControlModifier | Qt.KeyboardModifier.ShiftModifier) and key == Qt.Key.Key_Z:
                self._redo()
                return True

            # Ctrl+Y - Redo
            if modifiers == Qt.KeyboardModifier.ControlModifier and key == Qt.Key.Key_Y:
                self._redo()
                return True

            # Ctrl+C - Copy
            if modifiers == Qt.KeyboardModifier.ControlModifier and key == Qt.Key.Key_C:
                self._copy_layer()
                return True

            # Ctrl+X - Cut
            if modifiers == Qt.KeyboardModifier.ControlModifier and key == Qt.Key.Key_X:
                self._cut_layer()
                return True

            # Ctrl+V - Paste
            if modifiers == Qt.KeyboardModifier.ControlModifier and key == Qt.Key.Key_V:
                self._paste()
                return True

            # Ctrl+D - Clear Selection
            if modifiers == Qt.KeyboardModifier.ControlModifier and key == Qt.Key.Key_D:
                self._clear_selection()
                return True

        return super().eventFilter(obj, event)

    def _setup_panel(self):
        """Setup scribbler panel UI"""
        main_widget = QWidget()
        main_layout = QVBoxLayout()
        main_layout.setContentsMargins(5, 5, 5, 5)

        # Menu bar
        menubar = self._create_menubar()
        main_layout.addWidget(menubar)

        # Toolbar
        toolbar = self._create_toolbar()
        main_layout.addWidget(toolbar)

        # Main splitter
        main_splitter = QSplitter(Qt.Orientation.Horizontal)

        # Left side - Tools and layers
        left_widget = self._create_left_panel()
        main_splitter.addWidget(left_widget)

        # Center - Canvas
        self.canvas_widget = None  # Will be created in _create_default_canvas
        canvas_scroll = QScrollArea()
        canvas_scroll.setWidgetResizable(True)
        canvas_scroll.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.canvas_scroll = canvas_scroll
        main_splitter.addWidget(canvas_scroll)

        # Right side - Color and brush settings
        right_widget = self._create_right_panel()
        main_splitter.addWidget(right_widget)

        main_splitter.setSizes([200, 600, 250])
        main_layout.addWidget(main_splitter, 1)

        # Timeline widget (hidden by default)
        self.timeline_widget = TimelineWidget(self)
        self.timeline_widget.setVisible(False)
        self.timeline_widget.frame_selected.connect(self._on_timeline_frame_selected)
        self.timeline_widget.new_frame_btn.clicked.connect(self._add_new_frame)
        self.timeline_widget.duplicate_frame_btn.clicked.connect(self._duplicate_frame)
        self.timeline_widget.delete_frame_btn.clicked.connect(self._delete_frame)
        self.timeline_widget.move_left_btn.clicked.connect(self._move_frame_left)
        self.timeline_widget.move_right_btn.clicked.connect(self._move_frame_right)
        main_layout.addWidget(self.timeline_widget)

        main_widget.setLayout(main_layout)
        self.content_layout.addWidget(main_widget)

    def _create_toolbar(self) -> QWidget:
        """Create toolbar"""
        toolbar = QWidget()
        layout = QHBoxLayout()
        layout.setContentsMargins(0, 0, 0, 0)

        # Toolbar is now empty - all functionality moved to menus
        layout.addStretch()

        toolbar.setLayout(layout)
        return toolbar

    def _create_menubar(self) -> QMenuBar:
        """Create menu bar"""
        menubar = QMenuBar()

        # File menu
        file_menu = menubar.addMenu("File")

        # New
        new_action = QAction("New", self)
        new_action.setShortcut(QKeySequence.StandardKey.New)
        new_action.triggered.connect(self._new_canvas)
        file_menu.addAction(new_action)

        # Open
        open_action = QAction("Open", self)
        open_action.setShortcut(QKeySequence.StandardKey.Open)
        open_action.triggered.connect(self._open_file)
        file_menu.addAction(open_action)

        file_menu.addSeparator()

        # Save
        save_action = QAction("Save", self)
        save_action.setShortcut(QKeySequence.StandardKey.Save)
        save_action.triggered.connect(self._save_file)
        file_menu.addAction(save_action)

        # Save As
        save_as_action = QAction("Save As...", self)
        save_as_action.setShortcut(QKeySequence("Ctrl+Shift+S"))
        save_as_action.triggered.connect(self._save_as_file)
        file_menu.addAction(save_as_action)

        file_menu.addSeparator()

        # Export
        export_action = QAction("Export...", self)
        export_action.setShortcut(QKeySequence("Ctrl+E"))
        export_action.triggered.connect(self._export_image)
        file_menu.addAction(export_action)

        # Export Sprite Sheet
        export_spritesheet_action = QAction("Export Sprite Sheet...", self)
        export_spritesheet_action.triggered.connect(self._export_sprite_sheet)
        file_menu.addAction(export_spritesheet_action)

        # Export Animation (GIF)
        export_gif_action = QAction("Export Animation (GIF)...", self)
        export_gif_action.triggered.connect(self._export_gif)
        file_menu.addAction(export_gif_action)

        # Edit menu
        edit_menu = menubar.addMenu("Edit")

        # Undo (shortcuts set up separately in _setup_shortcuts)
        self.undo_action = QAction("Undo", self)
        self.undo_action.triggered.connect(self._undo)
        self.undo_action.setEnabled(False)
        edit_menu.addAction(self.undo_action)

        # Redo (shortcuts set up separately in _setup_shortcuts)
        self.redo_action = QAction("Redo", self)
        self.redo_action.triggered.connect(self._redo)
        self.redo_action.setEnabled(False)
        edit_menu.addAction(self.redo_action)

        edit_menu.addSeparator()

        # Cut (shortcuts set up separately in _setup_shortcuts)
        self.cut_action = QAction("Cut Layer", self)
        self.cut_action.triggered.connect(self._cut_layer)
        edit_menu.addAction(self.cut_action)

        # Copy (shortcuts set up separately in _setup_shortcuts)
        self.copy_action = QAction("Copy Layer", self)
        self.copy_action.triggered.connect(self._copy_layer)
        edit_menu.addAction(self.copy_action)

        # Paste (shortcuts set up separately in _setup_shortcuts)
        self.paste_action = QAction("Paste", self)
        self.paste_action.triggered.connect(self._paste)
        edit_menu.addAction(self.paste_action)

        # View menu
        view_menu = menubar.addMenu("View")

        # Reset View
        reset_view_action = QAction("Reset View", self)
        reset_view_action.setShortcut(QKeySequence("Ctrl+0"))
        reset_view_action.triggered.connect(self._reset_view)
        view_menu.addAction(reset_view_action)

        # Zoom In
        zoom_in_action = QAction("Zoom In", self)
        zoom_in_action.setShortcut(QKeySequence("Ctrl++"))
        zoom_in_action.triggered.connect(self._zoom_in)
        view_menu.addAction(zoom_in_action)

        # Zoom Out
        zoom_out_action = QAction("Zoom Out", self)
        zoom_out_action.setShortcut(QKeySequence("Ctrl+-"))
        zoom_out_action.triggered.connect(self._zoom_out)
        view_menu.addAction(zoom_out_action)

        view_menu.addSeparator()

        # Show Timeline
        self.show_timeline_action = QAction("Show Timeline", self)
        self.show_timeline_action.setCheckable(True)
        self.show_timeline_action.setChecked(False)
        self.show_timeline_action.triggered.connect(self._toggle_timeline)
        view_menu.addAction(self.show_timeline_action)

        # Onion Skinning
        self.onion_skinning_action = QAction("Onion Skinning", self)
        self.onion_skinning_action.setCheckable(True)
        self.onion_skinning_action.setChecked(False)
        self.onion_skinning_action.triggered.connect(self._toggle_onion_skinning)
        view_menu.addAction(self.onion_skinning_action)

        view_menu.addSeparator()

        # Animation Preview
        preview_action = QAction("Animation Preview", self)
        preview_action.triggered.connect(self._open_animation_preview)
        view_menu.addAction(preview_action)

        # Manage Animation Tags
        manage_tags_action = QAction("Manage Animation Tags...", self)
        manage_tags_action.triggered.connect(self._open_tag_manager)
        view_menu.addAction(manage_tags_action)

        return menubar

    def _setup_shortcuts(self):
        """Setup keyboard shortcuts using QShortcut for reliable triggering"""
        # Store shortcuts as instance variables to prevent garbage collection
        # Use ApplicationShortcut context to work globally

        # Undo - Ctrl+Z
        self.undo_shortcut = QShortcut(QKeySequence.StandardKey.Undo, self)
        self.undo_shortcut.activated.connect(self._undo)
        self.undo_shortcut.setContext(Qt.ShortcutContext.ApplicationShortcut)

        # Redo - Ctrl+Shift+Z or Ctrl+Y
        self.redo_shortcut = QShortcut(QKeySequence.StandardKey.Redo, self)
        self.redo_shortcut.activated.connect(self._redo)
        self.redo_shortcut.setContext(Qt.ShortcutContext.ApplicationShortcut)

        # Also add Ctrl+Y for redo (common alternative)
        self.redo_shortcut_alt = QShortcut(QKeySequence("Ctrl+Y"), self)
        self.redo_shortcut_alt.activated.connect(self._redo)
        self.redo_shortcut_alt.setContext(Qt.ShortcutContext.ApplicationShortcut)

        # Copy - Ctrl+C
        self.copy_shortcut = QShortcut(QKeySequence.StandardKey.Copy, self)
        self.copy_shortcut.activated.connect(self._copy_layer)
        self.copy_shortcut.setContext(Qt.ShortcutContext.ApplicationShortcut)

        # Cut - Ctrl+X
        self.cut_shortcut = QShortcut(QKeySequence.StandardKey.Cut, self)
        self.cut_shortcut.activated.connect(self._cut_layer)
        self.cut_shortcut.setContext(Qt.ShortcutContext.ApplicationShortcut)

        # Paste - Ctrl+V
        self.paste_shortcut = QShortcut(QKeySequence.StandardKey.Paste, self)
        self.paste_shortcut.activated.connect(self._paste)
        self.paste_shortcut.setContext(Qt.ShortcutContext.ApplicationShortcut)

        # Clear Selection - Ctrl+D
        self.clear_selection_shortcut = QShortcut(QKeySequence("Ctrl+D"), self)
        self.clear_selection_shortcut.activated.connect(self._clear_selection)
        self.clear_selection_shortcut.setContext(Qt.ShortcutContext.ApplicationShortcut)

        # Force enable all shortcuts
        self.undo_shortcut.setEnabled(True)
        self.redo_shortcut.setEnabled(True)
        self.redo_shortcut_alt.setEnabled(True)
        self.copy_shortcut.setEnabled(True)
        self.cut_shortcut.setEnabled(True)
        self.paste_shortcut.setEnabled(True)
        self.clear_selection_shortcut.setEnabled(True)

    def _create_left_panel(self) -> QWidget:
        """Create left panel with tools and layers"""
        widget = QWidget()
        layout = QVBoxLayout()
        layout.setContentsMargins(0, 0, 0, 0)

        # Tools group
        tools_group = QGroupBox("Tools")
        tools_layout = QVBoxLayout()

        # Get theme colors for styling
        theme_manager = get_theme_manager()
        theme = theme_manager.get_current_theme()
        c = theme.colors

        # Add styling to radio buttons to make selection clear
        radio_style = f"""
        QRadioButton {{
            padding: 5px;
            spacing: 5px;
            color: {c.text_primary};
        }}
        QRadioButton::indicator {{
            width: 15px;
            height: 15px;
        }}
        QRadioButton:checked {{
            background-color: {c.selection};
            color: {c.text_primary};
            border-radius: 3px;
            font-weight: bold;
        }}
        """

        self.tool_brush = QRadioButton("Brush")
        self.tool_brush.setStyleSheet(radio_style)
        self.tool_brush.setChecked(True)
        self.tool_brush.toggled.connect(lambda: self._set_tool("brush"))
        tools_layout.addWidget(self.tool_brush)

        self.tool_eraser = QRadioButton("Eraser")
        self.tool_eraser.setStyleSheet(radio_style)
        self.tool_eraser.toggled.connect(lambda: self._set_tool("eraser"))
        tools_layout.addWidget(self.tool_eraser)

        self.tool_move = QRadioButton("Move/Transform")
        self.tool_move.setStyleSheet(radio_style)
        self.tool_move.toggled.connect(lambda: self._set_tool("move"))
        tools_layout.addWidget(self.tool_move)

        self.tool_lasso = QRadioButton("Lasso Selection")
        self.tool_lasso.setStyleSheet(radio_style)
        self.tool_lasso.toggled.connect(lambda: self._set_tool("lasso"))
        tools_layout.addWidget(self.tool_lasso)

        self.tool_marquee = QRadioButton("Marquee Selection")
        self.tool_marquee.setStyleSheet(radio_style)
        self.tool_marquee.toggled.connect(lambda: self._set_tool("marquee"))
        tools_layout.addWidget(self.tool_marquee)

        self.tool_fill = QRadioButton("Fill Bucket")
        self.tool_fill.setStyleSheet(radio_style)
        self.tool_fill.toggled.connect(lambda: self._set_tool("fill"))
        tools_layout.addWidget(self.tool_fill)

        # Clear selection button
        clear_selection_btn = QPushButton("Clear Selection")
        clear_selection_btn.clicked.connect(self._clear_selection)
        tools_layout.addWidget(clear_selection_btn)

        tools_group.setLayout(tools_layout)
        layout.addWidget(tools_group)

        # Layers group
        layers_group = QGroupBox("Layers")
        layers_layout = QVBoxLayout()

        layer_buttons = QHBoxLayout()

        add_layer_btn = QPushButton("Add")
        add_layer_btn.clicked.connect(self._add_layer)
        layer_buttons.addWidget(add_layer_btn)

        remove_layer_btn = QPushButton("Remove")
        remove_layer_btn.clicked.connect(self._remove_layer)
        layer_buttons.addWidget(remove_layer_btn)

        duplicate_layer_btn = QPushButton("Duplicate")
        duplicate_layer_btn.clicked.connect(self._duplicate_layer)
        layer_buttons.addWidget(duplicate_layer_btn)

        layers_layout.addLayout(layer_buttons)

        self.layer_list = QListWidget()
        self.layer_list.currentRowChanged.connect(self._on_layer_selected)
        layers_layout.addWidget(self.layer_list)

        # Layer properties
        layer_props = QFormLayout()

        self.layer_visible = QCheckBox()
        self.layer_visible.setChecked(True)
        self.layer_visible.toggled.connect(self._update_layer_properties)
        layer_props.addRow("Visible:", self.layer_visible)

        self.layer_locked = QCheckBox()
        self.layer_locked.toggled.connect(self._update_layer_properties)
        layer_props.addRow("Locked:", self.layer_locked)

        self.layer_opacity = QSlider(Qt.Orientation.Horizontal)
        self.layer_opacity.setMinimum(0)
        self.layer_opacity.setMaximum(100)
        self.layer_opacity.setValue(100)
        self.layer_opacity.valueChanged.connect(self._update_layer_properties)
        layer_props.addRow("Opacity:", self.layer_opacity)

        self.layer_blend = QComboBox()
        self.layer_blend.addItems(BlendMode.get_all())
        self.layer_blend.currentTextChanged.connect(self._update_layer_properties)
        layer_props.addRow("Blend Mode:", self.layer_blend)

        layers_layout.addLayout(layer_props)

        layers_group.setLayout(layers_layout)
        layout.addWidget(layers_group, 1)

        widget.setLayout(layout)
        return widget

    def _create_right_panel(self) -> QWidget:
        """Create right panel with color and brush settings"""
        widget = QWidget()
        layout = QVBoxLayout()
        layout.setContentsMargins(0, 0, 0, 0)

        # Color group
        color_group = QGroupBox("Color")
        color_layout = QVBoxLayout()

        self.color_wheel = ColorWheelWidget()
        self.color_wheel.color_changed.connect(self._on_color_changed)
        color_layout.addWidget(self.color_wheel, alignment=Qt.AlignmentFlag.AlignCenter)

        color_btn_layout = QHBoxLayout()
        self.color_display = QPushButton()
        self.color_display.setFixedHeight(40)
        self.color_display.clicked.connect(self._pick_color_dialog)
        self._update_color_display()
        color_btn_layout.addWidget(self.color_display)
        color_layout.addLayout(color_btn_layout)

        color_group.setLayout(color_layout)
        layout.addWidget(color_group)

        # Brush settings group
        brush_group = QGroupBox("Brush Settings")
        brush_layout = QFormLayout()

        self.brush_size_slider = QSlider(Qt.Orientation.Horizontal)
        self.brush_size_slider.setMinimum(1)
        self.brush_size_slider.setMaximum(100)
        self.brush_size_slider.setValue(5)
        self.brush_size_slider.valueChanged.connect(self._update_brush_settings)
        brush_layout.addRow("Size:", self.brush_size_slider)

        self.brush_opacity_slider = QSlider(Qt.Orientation.Horizontal)
        self.brush_opacity_slider.setMinimum(0)
        self.brush_opacity_slider.setMaximum(100)
        self.brush_opacity_slider.setValue(100)
        self.brush_opacity_slider.valueChanged.connect(self._update_brush_settings)
        brush_layout.addRow("Opacity:", self.brush_opacity_slider)

        self.brush_antialias = QCheckBox()
        self.brush_antialias.setChecked(True)
        self.brush_antialias.toggled.connect(self._update_brush_settings)
        brush_layout.addRow("Anti-aliasing:", self.brush_antialias)

        brush_group.setLayout(brush_layout)
        layout.addWidget(brush_group)

        # Pen pressure group (hidden when fill bucket is selected)
        self.pressure_group = QGroupBox("Pen Pressure")
        pressure_layout = QVBoxLayout()

        self.pressure_size = QCheckBox("Affect Size")
        self.pressure_size.setChecked(True)
        self.pressure_size.toggled.connect(self._update_pressure_settings)
        pressure_layout.addWidget(self.pressure_size)

        self.pressure_opacity = QCheckBox("Affect Opacity")
        self.pressure_opacity.setChecked(True)
        self.pressure_opacity.toggled.connect(self._update_pressure_settings)
        pressure_layout.addWidget(self.pressure_opacity)

        self.pressure_group.setLayout(pressure_layout)
        layout.addWidget(self.pressure_group)

        # Stabilization group (hidden when fill bucket is selected)
        self.stabilization_group = QGroupBox("Stabilization")
        stabilization_layout = QFormLayout()

        self.stabilization_slider = QSlider(Qt.Orientation.Horizontal)
        self.stabilization_slider.setMinimum(0)
        self.stabilization_slider.setMaximum(100)
        self.stabilization_slider.setValue(0)
        self.stabilization_slider.valueChanged.connect(self._update_stabilization_settings)

        stabilization_label_layout = QHBoxLayout()
        self.stabilization_value_label = QLabel("0")
        stabilization_label_layout.addWidget(self.stabilization_slider)
        stabilization_label_layout.addWidget(self.stabilization_value_label)

        stabilization_layout.addRow("Amount:", stabilization_label_layout)

        self.stabilization_group.setLayout(stabilization_layout)
        layout.addWidget(self.stabilization_group)

        # Fill bucket settings group (shown only when fill bucket is selected)
        self.fill_settings_group = QGroupBox("Fill Bucket Settings")
        fill_settings_layout = QFormLayout()

        # Tolerance slider
        self.fill_tolerance_slider = QSlider(Qt.Orientation.Horizontal)
        self.fill_tolerance_slider.setMinimum(0)
        self.fill_tolerance_slider.setMaximum(255)
        self.fill_tolerance_slider.setValue(0)
        self.fill_tolerance_slider.valueChanged.connect(self._update_fill_settings)

        tolerance_label_layout = QHBoxLayout()
        self.fill_tolerance_value_label = QLabel("0")
        tolerance_label_layout.addWidget(self.fill_tolerance_slider)
        tolerance_label_layout.addWidget(self.fill_tolerance_value_label)

        fill_settings_layout.addRow("Tolerance:", tolerance_label_layout)

        # Close gaps slider
        self.fill_close_gaps_slider = QSlider(Qt.Orientation.Horizontal)
        self.fill_close_gaps_slider.setMinimum(0)
        self.fill_close_gaps_slider.setMaximum(10)
        self.fill_close_gaps_slider.setValue(0)
        self.fill_close_gaps_slider.valueChanged.connect(self._update_fill_settings)

        close_gaps_label_layout = QHBoxLayout()
        self.fill_close_gaps_value_label = QLabel("0")
        close_gaps_label_layout.addWidget(self.fill_close_gaps_slider)
        close_gaps_label_layout.addWidget(self.fill_close_gaps_value_label)

        fill_settings_layout.addRow("Close Gaps:", close_gaps_label_layout)

        self.fill_settings_group.setLayout(fill_settings_layout)
        layout.addWidget(self.fill_settings_group)
        self.fill_settings_group.setVisible(False)  # Hidden by default

        layout.addStretch()

        widget.setLayout(layout)
        return widget

    def _create_default_canvas(self):
        """Create default canvas"""
        self.canvas_widget = CanvasWidget(100, 100, QColor(200, 200, 200))
        self.canvas_widget.canvas_modified.connect(self._on_canvas_modified)
        self.canvas_widget.history_changed.connect(self._update_undo_redo_actions)
        # Install event filter on canvas to catch key events when it has focus
        self.canvas_widget.installEventFilter(self)

        if hasattr(self, 'canvas_scroll'):
            self.canvas_scroll.setWidget(self.canvas_widget)
            self._refresh_layer_list()

        # Connect timeline to canvas
        if hasattr(self, 'timeline_widget'):
            self.timeline_widget.set_canvas(self.canvas_widget)

    def _new_canvas(self):
        """Create new canvas"""
        dialog = NewCanvasDialog(self)
        if dialog.exec() == QDialog.DialogCode.Accepted:
            data = dialog.get_canvas_data()

            bg_color = data['bg_color'] if data['bg_enabled'] else None
            self.canvas_widget = CanvasWidget(data['width'], data['height'], bg_color)
            self.canvas_widget.set_background_enabled(data['bg_enabled'])
            self.canvas_widget.canvas_modified.connect(self._on_canvas_modified)
            self.canvas_widget.history_changed.connect(self._update_undo_redo_actions)
            self.canvas_widget.installEventFilter(self)

            self.canvas_scroll.setWidget(self.canvas_widget)
            self.current_file = None

            self._refresh_layer_list()
            self._update_brush_settings()
            self._update_pressure_settings()
            self._update_undo_redo_actions()

            # Reset timeline
            self.timeline_widget.set_canvas(self.canvas_widget)
            self.timeline_widget.update_display()

    def _open_file(self):
        """Open scribbler file or image file"""
        file_path, _ = QFileDialog.getOpenFileName(
            self,
            "Open File",
            str(self.project_root) if self.project_root else "",
            "All Supported Files (*.scribbler *.png *.jpg *.jpeg *.bmp);;Scribbler Files (*.scribbler);;Image Files (*.png *.jpg *.jpeg *.bmp);;All Files (*.*)"
        )

        if file_path:
            # Check file extension to determine how to load
            file_path_obj = Path(file_path)
            ext = file_path_obj.suffix.lower()

            if ext == '.scribbler':
                self._load_file(file_path)
            elif ext in ['.png', '.jpg', '.jpeg', '.bmp']:
                self._load_image_file(file_path)
            else:
                QMessageBox.warning(self, "Error", "Unsupported file format")

    def _save_file(self):
        """Save current file"""
        if not self.current_file:
            self._save_as_file()
            return

        self._do_save(self.current_file)

    def _save_as_file(self):
        """Save as new file"""
        file_path, _ = QFileDialog.getSaveFileName(
            self,
            "Save Scribbler File",
            str(self.project_root) if self.project_root else "",
            "Scribbler Files (*.scribbler)"
        )

        if file_path:
            if not file_path.endswith('.scribbler'):
                file_path += '.scribbler'

            self.current_file = file_path
            self._do_save(file_path)

    def _do_save(self, file_path: str):
        """Perform save operation"""
        try:
            # Save each layer as separate image
            layer_files = []
            base_path = Path(file_path).parent / Path(file_path).stem
            base_path.mkdir(exist_ok=True)

            for i, layer in enumerate(self.canvas_widget.layers):
                layer_file = str(base_path / f"layer_{i}.png")
                layer.canvas.save(layer_file)
                layer_files.append({
                    'file': f"{Path(file_path).stem}/layer_{i}.png",
                    'name': layer.name,
                    'visible': layer.visible,
                    'opacity': layer.opacity,
                    'blend_mode': layer.blend_mode,
                    'locked': layer.locked,
                    'offset_x': layer.offset_x,
                    'offset_y': layer.offset_y
                })

            data = {
                'width': self.canvas_widget.canvas_width,
                'height': self.canvas_widget.canvas_height,
                'has_background': self.canvas_widget.has_background,
                'bg_color': {
                    'r': self.canvas_widget.bg_color.red(),
                    'g': self.canvas_widget.bg_color.green(),
                    'b': self.canvas_widget.bg_color.blue()
                },
                'layers': layer_files,
                'current_layer': self.canvas_widget.current_layer_index
            }

            with open(file_path, 'w', encoding='utf-8') as f:
                json.dump(data, f, indent=2)

            QMessageBox.information(self, "Success", "File saved successfully")
        except Exception as e:
            QMessageBox.warning(self, "Error", f"Failed to save file:\n{str(e)}")

    def _load_image_file(self, file_path: str):
        """Load image file (PNG, JPG, etc.) as new canvas"""
        try:
            # Load image
            image = QImage(file_path)
            if image.isNull():
                QMessageBox.warning(self, "Error", "Failed to load image file")
                return

            # Create canvas with image dimensions
            width = image.width()
            height = image.height()

            # Create new canvas with transparent background
            self.canvas_widget = CanvasWidget(width, height, QColor(255, 255, 255))
            self.canvas_widget.set_background_enabled(True)
            self.canvas_widget.canvas_modified.connect(self._on_canvas_modified)
            self.canvas_widget.history_changed.connect(self._update_undo_redo_actions)
            self.canvas_widget.installEventFilter(self)

            # Clear default layer
            self.canvas_widget.layers.clear()

            # Create layer from image
            layer = Layer("Image", width, height)
            layer.canvas = QPixmap.fromImage(image)
            self.canvas_widget.layers.append(layer)
            self.canvas_widget.current_layer_index = 0

            self.canvas_scroll.setWidget(self.canvas_widget)
            self.current_file = None  # Not a scribbler file, so don't set current_file

            self._refresh_layer_list()
            self._update_brush_settings()
            self._update_pressure_settings()
            self._update_undo_redo_actions()

            QMessageBox.information(self, "Success", f"Image loaded successfully\nSize: {width}x{height}")
        except Exception as e:
            QMessageBox.warning(self, "Error", f"Failed to load image file:\n{str(e)}")
            import traceback
            traceback.print_exc()

    def _load_file(self, file_path: str):
        """Load scribbler file"""
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                data = json.load(f)

            # Create canvas
            bg_color = QColor(data['bg_color']['r'], data['bg_color']['g'], data['bg_color']['b'])
            self.canvas_widget = CanvasWidget(data['width'], data['height'], bg_color)
            self.canvas_widget.set_background_enabled(data['has_background'])
            self.canvas_widget.canvas_modified.connect(self._on_canvas_modified)
            self.canvas_widget.history_changed.connect(self._update_undo_redo_actions)
            self.canvas_widget.installEventFilter(self)

            # Clear default layer
            self.canvas_widget.layers.clear()

            # Load layers
            base_path = Path(file_path).parent
            for layer_data in data['layers']:
                layer = Layer(layer_data['name'], data['width'], data['height'])
                layer_file = base_path / layer_data['file']

                if layer_file.exists():
                    layer.canvas = QPixmap(str(layer_file))

                layer.visible = layer_data['visible']
                layer.opacity = layer_data['opacity']
                layer.blend_mode = layer_data['blend_mode']
                layer.locked = layer_data['locked']
                layer.offset_x = layer_data.get('offset_x', 0)
                layer.offset_y = layer_data.get('offset_y', 0)

                self.canvas_widget.layers.append(layer)

            self.canvas_widget.current_layer_index = data.get('current_layer', 0)

            self.canvas_scroll.setWidget(self.canvas_widget)
            self.current_file = file_path

            self._refresh_layer_list()
            self._update_brush_settings()
            self._update_pressure_settings()
            self._update_undo_redo_actions()

            # Update timeline
            self.timeline_widget.set_canvas(self.canvas_widget)
            self.timeline_widget.update_display()

            QMessageBox.information(self, "Success", "File loaded successfully")
        except Exception as e:
            QMessageBox.warning(self, "Error", f"Failed to load file:\n{str(e)}")
            import traceback
            traceback.print_exc()

    def _export_image(self):
        """Export as image"""
        file_path, _ = QFileDialog.getSaveFileName(
            self,
            "Export Image",
            str(self.project_root) if self.project_root else "",
            "PNG Files (*.png);;JPEG Files (*.jpg);;All Files (*.*)"
        )

        if file_path:
            if self.canvas_widget.save_to_file(file_path):
                QMessageBox.information(self, "Success", "Image exported successfully")
            else:
                QMessageBox.warning(self, "Error", "Failed to export image")

    def _export_sprite_sheet(self):
        """Export animation as sprite sheet"""
        if not self.canvas_widget:
            return

        # Show export dialog
        dialog = SpriteSheetExportDialog(self.canvas_widget, self)
        if dialog.exec() != QDialog.DialogCode.Accepted:
            return

        settings = dialog.get_settings()

        # Get file path
        file_path, _ = QFileDialog.getSaveFileName(
            self,
            "Export Sprite Sheet",
            str(self.project_root) if self.project_root else "",
            "PNG Files (*.png)"
        )

        if not file_path:
            return

        try:
            # Generate sprite sheet
            frames = self._collect_frames(settings['skip_duplicates'])

            if not frames:
                QMessageBox.warning(self, "Error", "No frames to export")
                return

            # Calculate layout
            layout_info = self._calculate_sprite_sheet_layout(frames, settings)

            # Create sprite sheet image
            sprite_sheet = self._create_sprite_sheet(frames, layout_info)

            # Save sprite sheet
            sprite_sheet.save(file_path, "PNG")

            # Export JSON if requested
            if settings['export_json']:
                json_path = file_path.rsplit('.', 1)[0] + '.json'
                self._export_sprite_sheet_json(json_path, layout_info, settings)

            QMessageBox.information(self, "Success", f"Sprite sheet exported to {file_path}")

        except Exception as e:
            QMessageBox.warning(self, "Error", f"Failed to export sprite sheet:\n{str(e)}")
            import traceback
            traceback.print_exc()

    def _collect_frames(self, skip_duplicates: bool):
        """Collect all frames for export"""
        frames = []
        frame_count = self.canvas_widget.get_frame_count()

        # Store current frame indices
        old_indices = [layer._current_frame_index for layer in self.canvas_widget.layers]

        previous_hash = None

        for frame_idx in range(frame_count):
            # Set all layers to this frame
            for layer in self.canvas_widget.layers:
                if frame_idx < len(layer.frames):
                    layer._current_frame_index = frame_idx
                else:
                    layer._current_frame_index = len(layer.frames) - 1

            # Composite this frame
            composite = self.canvas_widget.composite_layers()

            # Check for duplicates if needed
            if skip_duplicates:
                # Simple hash based on image data
                current_hash = hash(composite.toImage().constBits().asstring(composite.toImage().sizeInBytes()))
                if current_hash == previous_hash:
                    continue
                previous_hash = current_hash

            frames.append(composite.copy())

        # Restore layer frame indices
        for layer, old_index in zip(self.canvas_widget.layers, old_indices):
            layer._current_frame_index = old_index

        return frames

    def _calculate_sprite_sheet_layout(self, frames, settings):
        """Calculate sprite sheet layout"""
        frame_count = len(frames)
        frame_width = self.canvas_widget.canvas_width
        frame_height = self.canvas_widget.canvas_height

        layout_mode = settings['layout_mode']

        if layout_mode == "Fixed Columns":
            columns = settings['columns']
            rows = (frame_count + columns - 1) // columns
        elif layout_mode == "Fixed Rows":
            rows = settings['rows']
            columns = (frame_count + rows - 1) // rows
        elif layout_mode == "Single Row":
            columns = frame_count
            rows = 1
        elif layout_mode == "One Row Per Tag":
            # Each tag gets its own row
            if self.canvas_widget.animation_tags:
                rows = len(self.canvas_widget.animation_tags)
                columns = max(tag.end_frame - tag.start_frame + 1 for tag in self.canvas_widget.animation_tags)
            else:
                columns = frame_count
                rows = 1
        else:
            columns = frame_count
            rows = 1

        return {
            'columns': columns,
            'rows': rows,
            'frame_width': frame_width,
            'frame_height': frame_height,
            'frame_count': frame_count
        }

    def _create_sprite_sheet(self, frames, layout_info):
        """Create the sprite sheet image"""
        columns = layout_info['columns']
        rows = layout_info['rows']
        frame_width = layout_info['frame_width']
        frame_height = layout_info['frame_height']

        sheet_width = columns * frame_width
        sheet_height = rows * frame_height

        sprite_sheet = QImage(sheet_width, sheet_height, QImage.Format.Format_ARGB32)
        sprite_sheet.fill(Qt.GlobalColor.transparent)

        painter = QPainter(sprite_sheet)

        for idx, frame in enumerate(frames):
            row = idx // columns
            col = idx % columns
            x = col * frame_width
            y = row * frame_height
            painter.drawPixmap(x, y, frame)

        painter.end()

        return sprite_sheet

    def _export_sprite_sheet_json(self, json_path, layout_info, settings):
        """Export JSON metadata for sprite sheet"""
        metadata = {
            'frames': [],
            'meta': {
                'image': Path(json_path).stem + '.png',
                'format': 'RGBA8888',
                'size': {
                    'w': layout_info['columns'] * layout_info['frame_width'],
                    'h': layout_info['rows'] * layout_info['frame_height']
                },
                'scale': 1,
                'frameTags': []
            }
        }

        # Add frame data
        for idx in range(layout_info['frame_count']):
            row = idx // layout_info['columns']
            col = idx % layout_info['columns']
            x = col * layout_info['frame_width']
            y = row * layout_info['frame_height']

            metadata['frames'].append({
                'filename': f'frame_{idx}.png',
                'frame': {
                    'x': x,
                    'y': y,
                    'w': layout_info['frame_width'],
                    'h': layout_info['frame_height']
                },
                'rotated': False,
                'trimmed': False,
                'spriteSourceSize': {
                    'x': 0,
                    'y': 0,
                    'w': layout_info['frame_width'],
                    'h': layout_info['frame_height']
                },
                'sourceSize': {
                    'w': layout_info['frame_width'],
                    'h': layout_info['frame_height']
                }
            })

        # Add animation tags
        for tag in self.canvas_widget.animation_tags:
            metadata['meta']['frameTags'].append({
                'name': tag.name,
                'from': tag.start_frame,
                'to': tag.end_frame,
                'direction': tag.direction
            })

        # Write JSON file
        with open(json_path, 'w') as f:
            json.dump(metadata, f, indent=2)

    def _export_gif(self):
        """Export animation as GIF"""
        if not self.canvas_widget:
            return

        # Ask for FPS
        fps, ok = QInputDialog().getInt(
            self,
            "Export GIF",
            "Frames per second:",
            12, 1, 60, 1
        )

        if not ok:
            return

        # Get file path
        file_path, _ = QFileDialog.getSaveFileName(
            self,
            "Export GIF",
            str(self.project_root) if self.project_root else "",
            "GIF Files (*.gif)"
        )

        if not file_path:
            return

        try:
            # Collect all frames
            frames = self._collect_frames(skip_duplicates=False)

            if not frames:
                QMessageBox.warning(self, "Error", "No frames to export")
                return

            # Convert QPixmap frames to PIL Images
            pil_frames = []
            for frame in frames:
                # Convert QPixmap to QImage
                qimage = frame.toImage()

                # Convert to bytes
                buffer = qimage.constBits().asstring(qimage.sizeInBytes())

                # Create PIL Image
                from PIL import Image
                pil_image = Image.frombytes(
                    "RGBA",
                    (qimage.width(), qimage.height()),
                    buffer,
                    "raw",
                    "BGRA"
                )
                pil_frames.append(pil_image)

            # Calculate frame duration in milliseconds
            frame_duration = int(1000 / fps)

            # Save as GIF
            if pil_frames:
                pil_frames[0].save(
                    file_path,
                    save_all=True,
                    append_images=pil_frames[1:],
                    duration=frame_duration,
                    loop=0,
                    optimize=False
                )

                QMessageBox.information(self, "Success", f"GIF exported to {file_path}")

        except ImportError:
            QMessageBox.warning(
                self,
                "Error",
                "PIL (Pillow) is required for GIF export.\n"
                "Install with: pip install Pillow"
            )
        except Exception as e:
            QMessageBox.warning(self, "Error", f"Failed to export GIF:\n{str(e)}")
            import traceback
            traceback.print_exc()

    def _reset_view(self):
        """Reset view"""
        if self.canvas_widget:
            self.canvas_widget.reset_view()

    def _zoom_in(self):
        """Zoom in"""
        if self.canvas_widget:
            self.canvas_widget.zoom_level *= 1.2
            if self.canvas_widget.zoom_level > 20.0:
                self.canvas_widget.zoom_level = 20.0
            self.canvas_widget.update()

    def _zoom_out(self):
        """Zoom out"""
        if self.canvas_widget:
            self.canvas_widget.zoom_level *= 0.8
            if self.canvas_widget.zoom_level < 0.1:
                self.canvas_widget.zoom_level = 0.1
            self.canvas_widget.update()

    def _toggle_timeline(self):
        """Toggle timeline visibility"""
        if hasattr(self, 'timeline_widget'):
            is_visible = not self.timeline_widget.isVisible()
            self.timeline_widget.setVisible(is_visible)
            self.show_timeline_action.setChecked(is_visible)

    def _toggle_onion_skinning(self):
        """Toggle onion skinning"""
        if self.canvas_widget:
            self.canvas_widget.onion_skinning_enabled = not self.canvas_widget.onion_skinning_enabled
            self.onion_skinning_action.setChecked(self.canvas_widget.onion_skinning_enabled)
            self.canvas_widget.update()

    def _open_animation_preview(self):
        """Open animation preview window"""
        if self.canvas_widget:
            # Create as independent window with window flags (non-modal)
            # Keep reference to prevent garbage collection
            self.animation_preview = AnimationPreviewWidget(self.canvas_widget, None)
            self.animation_preview.setWindowFlags(Qt.WindowType.Window)
            self.animation_preview.setAttribute(Qt.WidgetAttribute.WA_DeleteOnClose)
            self.animation_preview.show()
            self.animation_preview.raise_()
            self.animation_preview.activateWindow()

    def _open_tag_manager(self):
        """Open animation tag manager"""
        if not self.canvas_widget:
            return

        dialog = AnimationTagManagerDialog(self.canvas_widget, self)
        dialog.exec()

    def _on_timeline_frame_selected(self, frame_index: int):
        """Handle timeline frame selection"""
        if self.canvas_widget:
            self.canvas_widget.set_current_frame(frame_index)
            self.canvas_widget.update()

    def _add_new_frame(self):
        """Add a new frame after current frame"""
        if self.canvas_widget:
            self.canvas_widget.add_frame_to_all_layers(self.canvas_widget.current_frame + 1)
            self.canvas_widget.set_current_frame(self.canvas_widget.current_frame + 1)
            if hasattr(self, 'timeline_widget'):
                self.timeline_widget.update_display()

    def _duplicate_frame(self):
        """Duplicate current frame"""
        if self.canvas_widget:
            self.canvas_widget.duplicate_current_frame()
            if hasattr(self, 'timeline_widget'):
                self.timeline_widget.update_display()

    def _delete_frame(self):
        """Delete current frame"""
        if self.canvas_widget:
            if self.canvas_widget.get_frame_count() > 1:
                self.canvas_widget.delete_current_frame()
                if hasattr(self, 'timeline_widget'):
                    self.timeline_widget.update_display()

    def _move_frame_left(self):
        """Move current frame to the left"""
        if self.canvas_widget and self.canvas_widget.current_frame > 0:
            current = self.canvas_widget.current_frame
            self.canvas_widget.move_frame(current, current - 1)
            if hasattr(self, 'timeline_widget'):
                self.timeline_widget.update_display()

    def _move_frame_right(self):
        """Move current frame to the right"""
        if self.canvas_widget:
            current = self.canvas_widget.current_frame
            if current < self.canvas_widget.get_frame_count() - 1:
                self.canvas_widget.move_frame(current, current + 1)
                if hasattr(self, 'timeline_widget'):
                    self.timeline_widget.update_display()

    def _set_tool(self, tool: str):
        """Set current tool and update UI visibility"""
        if self.canvas_widget:
            self.canvas_widget.current_tool = tool

        # Show/hide appropriate settings based on tool
        if tool == "fill":
            self.pressure_group.setVisible(False)
            self.stabilization_group.setVisible(False)
            self.fill_settings_group.setVisible(True)
        else:
            self.pressure_group.setVisible(True)
            self.stabilization_group.setVisible(True)
            self.fill_settings_group.setVisible(False)

    def _get_next_layer_number(self) -> int:
        """Get the next available layer number by finding gaps in existing numbers"""
        if not self.canvas_widget:
            return 1

        # Extract all layer numbers from existing layers
        existing_numbers = set()
        import re
        for layer in self.canvas_widget.layers:
            # Match "Layer X" pattern where X is a number
            match = re.match(r'Layer (\d+)', layer.name)
            if match:
                existing_numbers.add(int(match.group(1)))

        # Find the lowest available number starting from 1
        number = 1
        while number in existing_numbers:
            number += 1

        return number

    def _add_layer(self):
        """Add new layer with smart numbering at top of stack"""
        if self.canvas_widget:
            # Get the next available layer number
            layer_number = self._get_next_layer_number()
            new_layer = Layer(f"Layer {layer_number}", self.canvas_widget.canvas_width, self.canvas_widget.canvas_height)
            index = 0  # Add to top of stack

            # Use command pattern for undo/redo
            cmd = AddLayerCommand(self.canvas_widget, new_layer, index)
            self.canvas_widget.command_history.execute(cmd)
            self.canvas_widget.history_changed.emit()

            self._refresh_layer_list()
            self.layer_list.setCurrentRow(0)  # Select the new top layer
            self.canvas_widget.update()

    def _remove_layer(self):
        """Remove selected layer"""
        if self.canvas_widget and len(self.canvas_widget.layers) > 1:
            current_row = self.layer_list.currentRow()
            if current_row >= 0:
                # Current row directly corresponds to layer index
                actual_index = current_row
                layer = self.canvas_widget.layers[actual_index]

                # Use command pattern for undo/redo
                cmd = RemoveLayerCommand(self.canvas_widget, layer, actual_index)
                self.canvas_widget.command_history.execute(cmd)
                self.canvas_widget.history_changed.emit()

                self._refresh_layer_list()
                self.canvas_widget.update()

    def _duplicate_layer(self):
        """Duplicate selected layer"""
        if self.canvas_widget:
            current_row = self.layer_list.currentRow()
            if current_row >= 0:
                # Current row directly corresponds to layer index
                actual_index = current_row
                if 0 <= actual_index < len(self.canvas_widget.layers):
                    layer = self.canvas_widget.layers[actual_index]
                    new_layer = layer.copy()
                    # Insert duplicate right below the original
                    new_index = actual_index + 1

                    # Use command pattern for undo/redo
                    cmd = AddLayerCommand(self.canvas_widget, new_layer, new_index)
                    self.canvas_widget.command_history.execute(cmd)
                    self.canvas_widget.history_changed.emit()

                    self._refresh_layer_list()
                    # Select the new duplicated layer
                    self.layer_list.setCurrentRow(new_index)
                    self.canvas_widget.update()

    def _on_layer_selected(self, index: int):
        """Handle layer selection"""
        if self.canvas_widget and 0 <= index < len(self.canvas_widget.layers):
            self.canvas_widget.current_layer_index = index
            layer = self.canvas_widget.layers[index]

            # Update UI
            self.layer_visible.blockSignals(True)
            self.layer_locked.blockSignals(True)
            self.layer_opacity.blockSignals(True)
            self.layer_blend.blockSignals(True)

            self.layer_visible.setChecked(layer.visible)
            self.layer_locked.setChecked(layer.locked)
            self.layer_opacity.setValue(int(layer.opacity * 100))
            self.layer_blend.setCurrentText(layer.blend_mode)

            self.layer_visible.blockSignals(False)
            self.layer_locked.blockSignals(False)
            self.layer_opacity.blockSignals(False)
            self.layer_blend.blockSignals(False)

    def _update_layer_properties(self):
        """Update layer properties"""
        if self.canvas_widget:
            index = self.layer_list.currentRow()
            if 0 <= index < len(self.canvas_widget.layers):
                layer = self.canvas_widget.layers[index]
                layer.visible = self.layer_visible.isChecked()
                layer.locked = self.layer_locked.isChecked()
                layer.opacity = self.layer_opacity.value() / 100.0
                layer.blend_mode = self.layer_blend.currentText()
                self.canvas_widget.update()

    def _refresh_layer_list(self):
        """Refresh layer list (top to bottom display)"""
        self.layer_list.clear()
        if self.canvas_widget:
            # Display layers in order (top to bottom)
            for i, layer in enumerate(self.canvas_widget.layers):
                item = QListWidgetItem(layer.name)
                self.layer_list.addItem(item)

            # Select current layer
            self.layer_list.setCurrentRow(self.canvas_widget.current_layer_index)

    def _on_color_changed(self, color: QColor):
        """Handle color change"""
        if self.canvas_widget:
            self.canvas_widget.brush_color = color
            self._update_color_display()

    def _pick_color_dialog(self):
        """Pick color using dialog"""
        color = QColorDialog.getColor(
            self.canvas_widget.brush_color if self.canvas_widget else QColor(0, 0, 0),
            self,
            "Select Color"
        )

        if color.isValid() and self.canvas_widget:
            self.canvas_widget.brush_color = color
            self.color_wheel.set_color(color)
            self._update_color_display()

    def _update_color_display(self):
        """Update color display button"""
        if self.canvas_widget:
            color = self.canvas_widget.brush_color
            self.color_display.setStyleSheet(
                f"background-color: rgb({color.red()}, {color.green()}, {color.blue()});"
            )

    def _update_brush_settings(self):
        """Update brush settings"""
        if self.canvas_widget:
            self.canvas_widget.brush_size = self.brush_size_slider.value()
            self.canvas_widget.brush_opacity = self.brush_opacity_slider.value() / 100.0
            self.canvas_widget.brush_antialias = self.brush_antialias.isChecked()

    def _update_pressure_settings(self):
        """Update pressure settings"""
        if self.canvas_widget:
            self.canvas_widget.use_pressure_size = self.pressure_size.isChecked()
            self.canvas_widget.use_pressure_opacity = self.pressure_opacity.isChecked()

    def _update_stabilization_settings(self):
        """Update stabilization settings"""
        if self.canvas_widget:
            value = self.stabilization_slider.value()
            self.canvas_widget.stabilization_amount = value
            self.stabilization_value_label.setText(str(value))

    def _update_fill_settings(self):
        """Update fill bucket settings"""
        if self.canvas_widget:
            tolerance = self.fill_tolerance_slider.value()
            close_gaps = self.fill_close_gaps_slider.value()
            self.canvas_widget.fill_tolerance = tolerance
            self.canvas_widget.fill_close_gaps = close_gaps
            self.fill_tolerance_value_label.setText(str(tolerance))
            self.fill_close_gaps_value_label.setText(str(close_gaps))

    def _on_canvas_modified(self):
        """Handle canvas modification"""
        pass

    # ========================================================================
    # Undo/Redo Handlers
    # ========================================================================

    def _undo(self):
        """Handle undo action"""
        if self.canvas_widget and self.canvas_widget.undo():
            self._refresh_layer_list()

    def _redo(self):
        """Handle redo action"""
        if self.canvas_widget and self.canvas_widget.redo():
            self._refresh_layer_list()

    def _update_undo_redo_actions(self):
        """Update undo/redo action states"""
        if hasattr(self, 'undo_action') and hasattr(self, 'redo_action'):
            if self.canvas_widget:
                self.undo_action.setEnabled(self.canvas_widget.can_undo())
                self.redo_action.setEnabled(self.canvas_widget.can_redo())
            else:
                self.undo_action.setEnabled(False)
                self.redo_action.setEnabled(False)

    # ========================================================================
    # Clipboard Handlers
    # ========================================================================

    def _copy_layer(self):
        """Handle copy layer action"""
        if self.canvas_widget:
            layer = self.canvas_widget.copy_layer()
            if layer:
                self.clipboard_layer = layer

    def _cut_layer(self):
        """Handle cut layer action"""
        if self.canvas_widget:
            layer = self.canvas_widget.cut_layer()
            if layer:
                self.clipboard_layer = layer
                self._refresh_layer_list()
            else:
                QMessageBox.warning(self, "Cut", "Cannot cut the last layer")

    def _paste(self):
        """Handle paste action - supports both layer paste and external image paste"""
        if not self.canvas_widget:
            return

        # First try to paste from system clipboard (external images)
        from PyQt6.QtWidgets import QApplication
        clipboard = QApplication.clipboard()
        mime_data = clipboard.mimeData()

        # Try multiple methods to get image from clipboard
        image = None

        # Method 1: Try to get image directly
        if mime_data.hasImage():
            image = clipboard.image()

        # Method 2: Try pixmap (some browsers copy as pixmap)
        if (image is None or image.isNull()) and not clipboard.pixmap().isNull():
            image = clipboard.pixmap().toImage()

        # Method 3: Try to load from URLs (for images copied from browsers)
        if (image is None or image.isNull()) and mime_data.hasUrls():
            urls = mime_data.urls()
            if urls:
                url = urls[0]
                # Try to load image from URL
                if url.isLocalFile():
                    image = QImage(url.toLocalFile())

        # If we got a valid image, paste it
        if image is not None and not image.isNull():
            if self.canvas_widget.paste_image_from_clipboard(image):
                self._refresh_layer_list()
                return

        # If no external image, try to paste internal layer
        if self.clipboard_layer:
            self.canvas_widget.paste_layer(self.clipboard_layer)
            self._refresh_layer_list()

    def _clear_selection(self):
        """Clear the current selection"""
        if self.canvas_widget:
            self.canvas_widget.clear_selection()
