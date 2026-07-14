"""
Pixel Painter Tool
An Aseprite-like pixel art painting tool with layers, palettes, and pixel-perfect drawing
Features: Pen, Eraser, Bucket, Eyedropper, Layers with blend modes, Color palettes
"""

from PyQt6.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QLabel,
                             QPushButton, QListWidget, QListWidgetItem,
                             QFileDialog, QMessageBox, QDialog, QFormLayout,
                             QLineEdit, QComboBox, QDialogButtonBox, QSpinBox,
                             QDoubleSpinBox, QSplitter, QGroupBox, QGridLayout,
                             QScrollArea, QRadioButton, QButtonGroup, QFrame,
                             QSlider, QColorDialog, QCheckBox, QMenuBar, QMenu,
                             QToolButton, QSizePolicy)
from PyQt6.QtCore import Qt, QTimer, pyqtSignal, QSize, QRect, QPoint, QPointF, QMimeData, QRectF, QBuffer, QByteArray
from PyQt6.QtGui import (QPixmap, QImage, QPainter, QColor, QPen, QBrush,
                        QPainterPath, QTransform, QTabletEvent, QPalette,
                        QKeySequence, QAction, QShortcut, QClipboard, QBitmap, QRegion)
from pathlib import Path
import json
import sys
import struct
import zlib
from abc import ABC, abstractmethod
from typing import List, Optional
import base64

# Animation export libraries
try:
    from PIL import Image
    HAS_PIL = True
except ImportError:
    HAS_PIL = False

try:
    import cv2
    import numpy as np
    HAS_CV2 = True
except ImportError:
    HAS_CV2 = False

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

    def __init__(self, canvas: 'PixelCanvas', layer: 'Layer', index: int):
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

    def __init__(self, canvas: 'PixelCanvas', layer: 'Layer', index: int):
        self.canvas = canvas
        self.layer = layer.copy()
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
            'name': layer.name,
            'alpha_lock': layer.alpha_lock,
            'clipping_mask': layer.clipping_mask
        }
        self.after_state = None

    def execute(self):
        """Store after state"""
        self.after_state = {
            'visible': self.layer.visible,
            'opacity': self.layer.opacity,
            'blend_mode': self.layer.blend_mode,
            'locked': self.layer.locked,
            'name': self.layer.name,
            'alpha_lock': self.layer.alpha_lock,
            'clipping_mask': self.layer.clipping_mask
        }

    def undo(self):
        """Restore before state"""
        self.layer.visible = self.before_state['visible']
        self.layer.opacity = self.before_state['opacity']
        self.layer.blend_mode = self.before_state['blend_mode']
        self.layer.locked = self.before_state['locked']
        self.layer.name = self.before_state['name']
        self.layer.alpha_lock = self.before_state['alpha_lock']
        self.layer.clipping_mask = self.before_state['clipping_mask']

    def redo(self):
        """Restore after state"""
        if self.after_state:
            self.layer.visible = self.after_state['visible']
            self.layer.opacity = self.after_state['opacity']
            self.layer.blend_mode = self.after_state['blend_mode']
            self.layer.locked = self.after_state['locked']
            self.layer.name = self.after_state['name']
            self.layer.alpha_lock = self.after_state['alpha_lock']
            self.layer.clipping_mask = self.after_state['clipping_mask']


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
        self.redo_stack.clear()

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
    """Represents a single animation frame"""
    def __init__(self, width: int, height: int):
        self.canvas = QPixmap(width, height)
        self.canvas.fill(Qt.GlobalColor.transparent)
        self.duration = 100  # Duration in milliseconds

    def copy(self):
        """Create a copy of this frame"""
        new_frame = Frame(self.canvas.width(), self.canvas.height())
        new_frame.canvas = self.canvas.copy()
        new_frame.duration = self.duration
        return new_frame


class Layer:
    """Represents a drawing layer with animation frames"""
    def __init__(self, name: str, width: int, height: int):
        self.name = name
        self.visible = True
        self.opacity = 1.0
        self.blend_mode = BlendMode.NORMAL
        self.locked = False
        self.alpha_lock = False  # Lock transparency
        self.clipping_mask = False  # Use layer below as mask

        # Animation support
        self.frames = [Frame(width, height)]  # List of frames
        self.current_frame = 0

    @property
    def canvas(self):
        """Get current frame's canvas"""
        if 0 <= self.current_frame < len(self.frames):
            return self.frames[self.current_frame].canvas
        return self.frames[0].canvas

    @canvas.setter
    def canvas(self, pixmap: QPixmap):
        """Set current frame's canvas"""
        if 0 <= self.current_frame < len(self.frames):
            self.frames[self.current_frame].canvas = pixmap

    def resize(self, width: int, height: int):
        """Resize all frames in the layer"""
        for frame in self.frames:
            new_canvas = QPixmap(width, height)
            new_canvas.fill(Qt.GlobalColor.transparent)
            painter = QPainter(new_canvas)
            painter.drawPixmap(0, 0, frame.canvas)
            painter.end()
            frame.canvas = new_canvas

    def clear(self):
        """Clear the current frame"""
        if 0 <= self.current_frame < len(self.frames):
            self.frames[self.current_frame].canvas.fill(Qt.GlobalColor.transparent)

    def add_frame(self, index: int = -1):
        """Add a new frame"""
        new_frame = Frame(self.frames[0].canvas.width(), self.frames[0].canvas.height())
        if index < 0:
            self.frames.append(new_frame)
        else:
            self.frames.insert(index, new_frame)

    def duplicate_frame(self, index: int):
        """Duplicate a frame"""
        if 0 <= index < len(self.frames):
            new_frame = self.frames[index].copy()
            self.frames.insert(index + 1, new_frame)

    def remove_frame(self, index: int):
        """Remove a frame"""
        if len(self.frames) > 1 and 0 <= index < len(self.frames):
            self.frames.pop(index)
            if self.current_frame >= len(self.frames):
                self.current_frame = len(self.frames) - 1

    def copy(self):
        """Create a copy of this layer"""
        new_layer = Layer(f"{self.name} Copy", self.frames[0].canvas.width(), self.frames[0].canvas.height())
        new_layer.frames = [frame.copy() for frame in self.frames]
        new_layer.current_frame = self.current_frame
        new_layer.visible = self.visible
        new_layer.opacity = self.opacity
        new_layer.blend_mode = self.blend_mode
        new_layer.locked = self.locked
        new_layer.alpha_lock = self.alpha_lock
        new_layer.clipping_mask = self.clipping_mask
        return new_layer


class AnimationTag:
    """Represents an animation tag for a range of frames"""
    def __init__(self, name: str, start_frame: int, end_frame: int):
        self.name = name
        self.start_frame = start_frame
        self.end_frame = end_frame
        self.direction = "forward"  # forward, reverse, pingpong

    def to_dict(self):
        """Convert to dictionary"""
        return {
            'name': self.name,
            'start_frame': self.start_frame,
            'end_frame': self.end_frame,
            'direction': self.direction
        }

    @staticmethod
    def from_dict(data: dict):
        """Load from dictionary"""
        tag = AnimationTag(data['name'], data['start_frame'], data['end_frame'])
        tag.direction = data.get('direction', 'forward')
        return tag


class ColorPalette:
    """Manages a color palette"""
    def __init__(self, name: str = "Default"):
        self.name = name
        self.colors: List[QColor] = []
        self._init_default_palette()

    def _init_default_palette(self):
        """Initialize with default pixel art palette"""
        default_colors = [
            "#000000", "#1a1c2c", "#5d275d", "#b13e53", "#ef7d57",
            "#ffcd75", "#a7f070", "#38b764", "#257179", "#29366f",
            "#3b5dc9", "#41a6f6", "#73eff7", "#ffffff", "#f4f4f4",
            "#94b0c2", "#566c86", "#333c57"
        ]
        for color_str in default_colors:
            self.colors.append(QColor(color_str))

    def add_color(self, color: QColor):
        """Add a color to the palette"""
        if color not in self.colors:
            self.colors.append(color)

    def remove_color(self, index: int):
        """Remove a color from the palette"""
        if 0 <= index < len(self.colors):
            self.colors.pop(index)

    def get_color(self, index: int) -> Optional[QColor]:
        """Get a color by index"""
        if 0 <= index < len(self.colors):
            return self.colors[index]
        return None

    def to_dict(self) -> dict:
        """Convert palette to dictionary"""
        return {
            'name': self.name,
            'colors': [c.name() for c in self.colors]
        }

    @staticmethod
    def from_dict(data: dict) -> 'ColorPalette':
        """Load palette from dictionary"""
        palette = ColorPalette(data['name'])
        palette.colors = [QColor(c) for c in data['colors']]
        return palette

    @staticmethod
    def load_aseprite_palette(filepath: str) -> 'ColorPalette':
        """Load Aseprite palette file (.ase, .aseprite, .gpl)"""
        path = Path(filepath)
        palette = ColorPalette(path.stem)

        if path.suffix.lower() == '.gpl':
            # GIMP palette format (also used by Aseprite)
            with open(filepath, 'r') as f:
                lines = f.readlines()
                for line in lines:
                    line = line.strip()
                    if line and not line.startswith('#') and not line.startswith('GIMP'):
                        parts = line.split()
                        if len(parts) >= 3:
                            try:
                                r = int(parts[0])
                                g = int(parts[1])
                                b = int(parts[2])
                                palette.colors.append(QColor(r, g, b))
                            except ValueError:
                                continue

        return palette


class PixelCanvas(QWidget):
    """
    Canvas widget for pixel art drawing
    """

    canvas_modified = pyqtSignal()
    history_changed = pyqtSignal()
    color_picked = pyqtSignal(QColor)  # Signal when eyedropper picks a color

    def __init__(self, width: int = 32, height: int = 32, parent=None):
        super().__init__(parent)

        # Canvas properties
        self.canvas_width = width
        self.canvas_height = height

        # Layers
        self.layers = []
        self.current_layer_index = 0

        # Animation
        self.current_frame = 0
        self.animation_tags = []  # List of AnimationTag objects
        self.onion_skinning = False
        self.onion_prev_frames = 1
        self.onion_next_frames = 1
        self.onion_opacity = 0.3

        # Command history
        self.command_history = CommandHistory()

        # Drawing state
        self.drawing_command = None
        self.layer_before_drawing = None

        # Create initial layer
        self.add_layer("Layer 1")

        # View properties
        self.zoom_level = 8.0  # Start zoomed in for pixel art
        self.pan_offset = QPoint(0, 0)
        self.show_grid = True
        self.show_pixel_grid = True
        self.show_tile_grid = False
        self.tile_grid_width = 8  # Tile grid size in pixels
        self.tile_grid_height = 8

        # Drawing state
        self.is_drawing = False
        self.last_point = None
        self.current_tool = "pen"  # "pen", "eraser", "bucket", "eyedropper"

        # Pen properties
        self.pen_size = 1  # Pixel size
        self.pen_color = QColor(0, 0, 0)
        self.pen_antialias = False  # No antialiasing for pixel perfect

        # UI setup
        self.setMinimumSize(400, 400)
        self.setMouseTracking(True)
        self.setFocusPolicy(Qt.FocusPolicy.StrongFocus)

        # Set background
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

    def get_current_layer(self) -> Layer:
        """Get the currently active layer"""
        if 0 <= self.current_layer_index < len(self.layers):
            return self.layers[self.current_layer_index]
        return None

    def get_frame_count(self) -> int:
        """Get the maximum number of frames across all layers"""
        if not self.layers:
            return 1
        return max(len(layer.frames) for layer in self.layers)

    def set_frame(self, frame_index: int):
        """Set the current frame for all layers"""
        self.current_frame = frame_index
        for layer in self.layers:
            layer.current_frame = frame_index
        self.update()

    def add_frame_to_all_layers(self, index: int = -1):
        """Add a frame to all layers"""
        for layer in self.layers:
            layer.add_frame(index)
        if index < 0:
            self.current_frame = len(self.layers[0].frames) - 1
        else:
            self.current_frame = index
        self.set_frame(self.current_frame)

    def duplicate_frame_all_layers(self, index: int):
        """Duplicate a frame across all layers"""
        for layer in self.layers:
            layer.duplicate_frame(index)
        self.current_frame = index + 1
        self.set_frame(self.current_frame)

    def remove_frame_from_all_layers(self, index: int):
        """Remove a frame from all layers"""
        if self.get_frame_count() <= 1:
            return
        for layer in self.layers:
            layer.remove_frame(index)
        if self.current_frame >= self.get_frame_count():
            self.current_frame = self.get_frame_count() - 1
        self.set_frame(self.current_frame)

    def move_frame(self, from_index: int, to_index: int):
        """Move a frame"""
        if from_index == to_index:
            return
        for layer in self.layers:
            if 0 <= from_index < len(layer.frames) and 0 <= to_index < len(layer.frames):
                frame = layer.frames.pop(from_index)
                layer.frames.insert(to_index, frame)
        self.current_frame = to_index
        self.set_frame(self.current_frame)

    def set_canvas_size(self, width: int, height: int):
        """Set canvas size"""
        self.canvas_width = width
        self.canvas_height = height

        # Resize all layers
        for layer in self.layers:
            layer.resize(width, height)

        self.update()

    def composite_layers(self, include_onion_skinning: bool = True) -> QPixmap:
        """Composite all layers into a single pixmap"""
        result = QPixmap(self.canvas_width, self.canvas_height)
        result.fill(Qt.GlobalColor.transparent)

        painter = QPainter(result)

        # Draw onion skinning (previous frames)
        if include_onion_skinning and self.onion_skinning:
            for offset in range(self.onion_prev_frames, 0, -1):
                frame_index = self.current_frame - offset
                if frame_index >= 0:
                    onion_composite = self._composite_frame(frame_index)

                    # Tint previous frames red
                    tinted = QPixmap(onion_composite.size())
                    tinted.fill(Qt.GlobalColor.transparent)
                    tint_painter = QPainter(tinted)

                    # Draw the original frame
                    tint_painter.drawPixmap(0, 0, onion_composite)

                    # Apply red tint overlay
                    tint_painter.setCompositionMode(QPainter.CompositionMode.CompositionMode_SourceAtop)
                    tint_painter.fillRect(tinted.rect(), QColor(255, 0, 0, 100))
                    tint_painter.end()

                    # Draw tinted frame with reduced opacity
                    painter.setOpacity(self.onion_opacity * (1.0 - offset * 0.3 / self.onion_prev_frames))
                    painter.setCompositionMode(QPainter.CompositionMode.CompositionMode_SourceOver)
                    painter.drawPixmap(0, 0, tinted)

        # Draw current frame layers from bottom to top (reverse order)
        for i in range(len(self.layers) - 1, -1, -1):
            layer = self.layers[i]
            if not layer.visible:
                continue

            # Handle clipping mask
            if layer.clipping_mask and i < len(self.layers) - 1:
                # Use the layer below as a mask
                mask_layer = self.layers[i + 1]
                if mask_layer.visible:
                    # Create a temporary pixmap for the clipped result
                    clipped = QPixmap(self.canvas_width, self.canvas_height)
                    clipped.fill(Qt.GlobalColor.transparent)

                    clip_painter = QPainter(clipped)
                    clip_painter.setCompositionMode(QPainter.CompositionMode.CompositionMode_Source)
                    clip_painter.drawPixmap(0, 0, layer.canvas)
                    clip_painter.setCompositionMode(QPainter.CompositionMode.CompositionMode_DestinationIn)
                    clip_painter.drawPixmap(0, 0, mask_layer.canvas)
                    clip_painter.end()

                    painter.setOpacity(layer.opacity)
                    self._set_blend_mode(painter, layer.blend_mode)
                    painter.drawPixmap(0, 0, clipped)
                continue

            # Set opacity
            painter.setOpacity(layer.opacity)

            # Set blend mode
            self._set_blend_mode(painter, layer.blend_mode)

            # Draw layer
            painter.drawPixmap(0, 0, layer.canvas)

        # Draw onion skinning (next frames)
        if include_onion_skinning and self.onion_skinning:
            for offset in range(1, self.onion_next_frames + 1):
                frame_index = self.current_frame + offset
                if frame_index < self.get_frame_count():
                    onion_composite = self._composite_frame(frame_index)

                    # Tint next frames blue
                    tinted = QPixmap(onion_composite.size())
                    tinted.fill(Qt.GlobalColor.transparent)
                    tint_painter = QPainter(tinted)

                    # Draw the original frame
                    tint_painter.drawPixmap(0, 0, onion_composite)

                    # Apply blue tint overlay
                    tint_painter.setCompositionMode(QPainter.CompositionMode.CompositionMode_SourceAtop)
                    tint_painter.fillRect(tinted.rect(), QColor(0, 0, 255, 100))
                    tint_painter.end()

                    # Draw tinted frame with reduced opacity
                    painter.setOpacity(self.onion_opacity * (1.0 - offset * 0.3 / self.onion_next_frames))
                    painter.setCompositionMode(QPainter.CompositionMode.CompositionMode_SourceOver)
                    painter.drawPixmap(0, 0, tinted)

        painter.end()
        return result

    def _composite_frame(self, frame_index: int) -> QPixmap:
        """Composite a specific frame"""
        result = QPixmap(self.canvas_width, self.canvas_height)
        result.fill(Qt.GlobalColor.transparent)

        painter = QPainter(result)

        # Save current frame states
        saved_frames = [layer.current_frame for layer in self.layers]

        # Set to target frame
        for layer in self.layers:
            if frame_index < len(layer.frames):
                layer.current_frame = frame_index

        # Draw layers
        for i in range(len(self.layers) - 1, -1, -1):
            layer = self.layers[i]
            if not layer.visible:
                continue

            painter.setOpacity(layer.opacity)
            self._set_blend_mode(painter, layer.blend_mode)
            painter.drawPixmap(0, 0, layer.canvas)

        painter.end()

        # Restore frame states
        for layer, saved_frame in zip(self.layers, saved_frames):
            layer.current_frame = saved_frame

        return result

    def _set_blend_mode(self, painter: QPainter, blend_mode: str):
        """Set the painter's blend mode"""
        if blend_mode == BlendMode.MULTIPLY:
            painter.setCompositionMode(QPainter.CompositionMode.CompositionMode_Multiply)
        elif blend_mode == BlendMode.OVERLAY:
            painter.setCompositionMode(QPainter.CompositionMode.CompositionMode_Overlay)
        elif blend_mode == BlendMode.SOFTLIGHT:
            painter.setCompositionMode(QPainter.CompositionMode.CompositionMode_SoftLight)
        else:  # NORMAL
            painter.setCompositionMode(QPainter.CompositionMode.CompositionMode_SourceOver)

    def paintEvent(self, event):
        """Paint the canvas"""
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

        # Draw checkerboard pattern for transparency
        self._draw_checkerboard(painter, canvas_x, canvas_y, scaled_width, scaled_height)

        # Composite and draw layers
        composite = self.composite_layers()

        # Scale canvas with nearest neighbor for pixel perfect
        if self.zoom_level != 1.0:
            composite = composite.scaled(scaled_width, scaled_height,
                                        Qt.AspectRatioMode.KeepAspectRatio,
                                        Qt.TransformationMode.FastTransformation)

        painter.drawPixmap(canvas_x, canvas_y, composite)

        # Draw pixel grid if zoomed in enough
        if self.show_pixel_grid and self.zoom_level >= 4.0:
            self._draw_pixel_grid(painter, canvas_x, canvas_y, scaled_width, scaled_height)

        # Draw tile grid
        if self.show_tile_grid:
            self._draw_tile_grid(painter, canvas_x, canvas_y, scaled_width, scaled_height)

        # Draw border
        painter.setPen(QPen(QColor(c.border), 2))
        painter.drawRect(canvas_x, canvas_y, scaled_width, scaled_height)

    def _draw_checkerboard(self, painter: QPainter, x: int, y: int, width: int, height: int):
        """Draw checkerboard pattern for transparent background"""
        checker_size = max(1, int(10 * self.zoom_level / 8))
        theme_manager = get_theme_manager()
        theme = theme_manager.get_current_theme()

        # Use theme colors for checkerboard
        light_color = QColor(theme.colors.surface_hover)
        dark_color = QColor(theme.colors.surface)

        cols = (width // checker_size) + 1
        rows = (height // checker_size) + 1

        for row in range(rows):
            for col in range(cols):
                color = light_color if (row + col) % 2 == 0 else dark_color
                painter.fillRect(x + col * checker_size, y + row * checker_size,
                               checker_size, checker_size, color)

    def _draw_pixel_grid(self, painter: QPainter, x: int, y: int, width: int, height: int):
        """Draw pixel grid"""
        theme_manager = get_theme_manager()
        theme = theme_manager.get_current_theme()

        grid_color = QColor(theme.colors.border)
        grid_color.setAlpha(64)
        painter.setPen(QPen(grid_color, 1))

        pixel_size = self.zoom_level

        # Draw vertical lines
        for i in range(self.canvas_width + 1):
            px = x + int(i * pixel_size)
            painter.drawLine(px, y, px, y + height)

        # Draw horizontal lines
        for i in range(self.canvas_height + 1):
            py = y + int(i * pixel_size)
            painter.drawLine(x, py, x + width, py)

    def _draw_tile_grid(self, painter: QPainter, x: int, y: int, width: int, height: int):
        """Draw tile grid overlay"""
        theme_manager = get_theme_manager()
        theme = theme_manager.get_current_theme()

        grid_color = QColor(theme.colors.accent_color)
        grid_color.setAlpha(128)
        painter.setPen(QPen(grid_color, 2))

        # Draw vertical lines
        for i in range(0, self.canvas_width + 1, self.tile_grid_width):
            px = x + int(i * self.zoom_level)
            painter.drawLine(px, y, px, y + height)

        # Draw horizontal lines
        for i in range(0, self.canvas_height + 1, self.tile_grid_height):
            py = y + int(i * self.zoom_level)
            painter.drawLine(x, py, x + width, py)

    def mousePressEvent(self, event):
        """Handle mouse press"""
        if event.button() == Qt.MouseButton.LeftButton:
            canvas_pos = self._screen_to_canvas(event.pos())

            if self.current_tool == "eyedropper":
                # Pick color
                color = self._pick_color_at(canvas_pos)
                if color:
                    self.color_picked.emit(color)

            elif self.current_tool == "bucket":
                # Fill bucket
                if self._is_point_in_canvas(canvas_pos):
                    layer = self.get_current_layer()
                    if layer and not layer.locked:
                        self.layer_before_drawing = layer.canvas.copy()
                        self._flood_fill(canvas_pos, self.pen_color)
                        cmd = DrawCommand(layer, self.layer_before_drawing)
                        self.command_history.execute(cmd)
                        self.layer_before_drawing = None
                        self.history_changed.emit()
                        self.canvas_modified.emit()

            else:
                # Drawing mode (pen/eraser)
                if self._is_point_in_canvas(canvas_pos):
                    self.is_drawing = True
                    self.last_point = canvas_pos

                    layer = self.get_current_layer()
                    if layer and not layer.locked:
                        self.layer_before_drawing = layer.canvas.copy()

                    self._draw_pixel(canvas_pos)

        elif event.button() == Qt.MouseButton.MiddleButton:
            self.last_point = event.pos()

    def mouseMoveEvent(self, event):
        """Handle mouse move"""
        if event.buttons() & Qt.MouseButton.LeftButton:
            canvas_pos = self._screen_to_canvas(event.pos())

            if self.is_drawing:
                if self.last_point:
                    self._draw_line(self.last_point, canvas_pos)
                self.last_point = canvas_pos

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
            if self.is_drawing:
                self.is_drawing = False
                self.last_point = None

                # Create undo command
                layer = self.get_current_layer()
                if layer and self.layer_before_drawing is not None:
                    cmd = DrawCommand(layer, self.layer_before_drawing)
                    self.command_history.execute(cmd)
                    self.layer_before_drawing = None
                    self.history_changed.emit()

                self.canvas_modified.emit()

        elif event.button() == Qt.MouseButton.MiddleButton:
            self.last_point = None

    def wheelEvent(self, event):
        """Handle mouse wheel for zoom"""
        delta = event.angleDelta().y() / 120.0
        zoom_factor = 1.2 if delta > 0 else 0.8

        new_zoom = self.zoom_level * zoom_factor
        if 1.0 <= new_zoom <= 64.0:
            self.zoom_level = new_zoom
            self.update()

    def _screen_to_canvas(self, screen_pos: QPoint) -> QPointF:
        """Convert screen coordinates to canvas coordinates"""
        scaled_width = int(self.canvas_width * self.zoom_level)
        scaled_height = int(self.canvas_height * self.zoom_level)

        canvas_x = (self.width() - scaled_width) // 2 + self.pan_offset.x()
        canvas_y = (self.height() - scaled_height) // 2 + self.pan_offset.y()

        x = (screen_pos.x() - canvas_x) / self.zoom_level
        y = (screen_pos.y() - canvas_y) / self.zoom_level

        return QPointF(x, y)

    def _is_point_in_canvas(self, canvas_pos: QPointF) -> bool:
        """Check if point is within canvas bounds"""
        return (0 <= canvas_pos.x() < self.canvas_width and
                0 <= canvas_pos.y() < self.canvas_height)

    def _draw_pixel(self, pos: QPointF):
        """Draw a single pixel or pixel block"""
        layer = self.get_current_layer()
        if not layer or layer.locked:
            return

        x = int(pos.x())
        y = int(pos.y())

        if x < 0 or x >= self.canvas_width or y < 0 or y >= self.canvas_height:
            return

        painter = QPainter(layer.canvas)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing, self.pen_antialias)

        if self.current_tool == "eraser":
            painter.setCompositionMode(QPainter.CompositionMode.CompositionMode_Clear)
            painter.setPen(Qt.PenStyle.NoPen)
            painter.setBrush(QBrush(QColor(0, 0, 0, 255)))
        else:  # pen
            # Handle alpha lock
            if layer.alpha_lock:
                painter.setCompositionMode(QPainter.CompositionMode.CompositionMode_SourceAtop)
            else:
                painter.setCompositionMode(QPainter.CompositionMode.CompositionMode_SourceOver)

            painter.setPen(Qt.PenStyle.NoPen)
            painter.setBrush(QBrush(self.pen_color))

        # Draw pixel block based on pen size
        for dy in range(self.pen_size):
            for dx in range(self.pen_size):
                px = x + dx
                py = y + dy
                if 0 <= px < self.canvas_width and 0 <= py < self.canvas_height:
                    painter.drawRect(px, py, 1, 1)

        painter.end()
        self.update()

    def _draw_line(self, start: QPointF, end: QPointF):
        """Draw a line between two points"""
        # Bresenham's line algorithm for pixel-perfect lines
        x0, y0 = int(start.x()), int(start.y())
        x1, y1 = int(end.x()), int(end.y())

        dx = abs(x1 - x0)
        dy = abs(y1 - y0)
        sx = 1 if x0 < x1 else -1
        sy = 1 if y0 < y1 else -1
        err = dx - dy

        while True:
            self._draw_pixel(QPointF(x0, y0))

            if x0 == x1 and y0 == y1:
                break

            e2 = 2 * err
            if e2 > -dy:
                err -= dy
                x0 += sx
            if e2 < dx:
                err += dx
                y0 += sy

    def _pick_color_at(self, pos: QPointF) -> Optional[QColor]:
        """Pick color at position from composite"""
        x = int(pos.x())
        y = int(pos.y())

        if x < 0 or x >= self.canvas_width or y < 0 or y >= self.canvas_height:
            return None

        composite = self.composite_layers()
        image = composite.toImage()
        return image.pixelColor(x, y)

    def _flood_fill(self, start_point: QPointF, fill_color: QColor):
        """Flood fill algorithm"""
        layer = self.get_current_layer()
        if not layer or layer.locked:
            return

        x = int(start_point.x())
        y = int(start_point.y())

        if x < 0 or x >= self.canvas_width or y < 0 or y >= self.canvas_height:
            return

        image = layer.canvas.toImage()
        target_color = QColor(image.pixelColor(x, y))

        if target_color.rgba() == fill_color.rgba():
            return

        # Stack-based flood fill
        stack = [(x, y)]
        visited = set()

        painter = QPainter(layer.canvas)
        if layer.alpha_lock:
            painter.setCompositionMode(QPainter.CompositionMode.CompositionMode_SourceAtop)
        else:
            painter.setCompositionMode(QPainter.CompositionMode.CompositionMode_Source)
        painter.setPen(Qt.PenStyle.NoPen)
        painter.setBrush(QBrush(fill_color))

        while stack:
            cx, cy = stack.pop()

            if cx < 0 or cx >= self.canvas_width or cy < 0 or cy >= self.canvas_height:
                continue

            if (cx, cy) in visited:
                continue

            current_color = QColor(image.pixelColor(cx, cy))
            if current_color.rgba() != target_color.rgba():
                continue

            visited.add((cx, cy))
            painter.drawRect(cx, cy, 1, 1)
            image.setPixelColor(cx, cy, fill_color)

            stack.append((cx + 1, cy))
            stack.append((cx - 1, cy))
            stack.append((cx, cy + 1))
            stack.append((cx, cy - 1))

        painter.end()
        self.update()

    def reset_view(self):
        """Reset zoom and pan"""
        self.zoom_level = 8.0
        self.pan_offset = QPoint(0, 0)
        self.update()

    def save_to_file(self, file_path: str) -> bool:
        """Save canvas to image file"""
        composite = self.composite_layers()
        return composite.save(file_path)

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


class ColorWheelWidget(QWidget):
    """Color wheel widget for color selection"""

    color_changed = pyqtSignal(QColor)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.current_color = QColor(0, 0, 0)
        self.setMinimumSize(150, 150)
        self.setMaximumSize(250, 250)

    def paintEvent(self, event):
        """Draw color wheel"""
        import math

        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)

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

        # Draw current color indicator
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
        import math

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


class PaletteWidget(QWidget):
    """Color palette widget"""

    color_selected = pyqtSignal(QColor)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.palette = ColorPalette()
        self.selected_index = -1
        self.color_size = 24
        self.spacing = 2
        self.setMinimumHeight(100)
        # Initialize size after a delay to ensure proper width
        QTimer.singleShot(0, self._update_size)

    def sizeHint(self):
        """Calculate size hint based on number of colors"""
        if not self.palette.colors:
            return QSize(200, 100)

        # Calculate how many rows we need
        colors_per_row = max(1, self.width() // (self.color_size + self.spacing))
        num_rows = (len(self.palette.colors) + colors_per_row - 1) // colors_per_row

        # Calculate total height needed
        height = num_rows * (self.color_size + self.spacing) + self.spacing
        return QSize(self.width(), height)

    def minimumSizeHint(self):
        """Minimum size hint"""
        return QSize(100, 100)

    def resizeEvent(self, event):
        """Handle resize to update size hint"""
        super().resizeEvent(event)
        self._update_size()

    def paintEvent(self, event):
        """Draw palette"""
        painter = QPainter(self)

        theme_manager = get_theme_manager()
        theme = theme_manager.get_current_theme()

        # Calculate grid
        colors_per_row = self.width() // (self.color_size + self.spacing)
        if colors_per_row == 0:
            colors_per_row = 1

        for i, color in enumerate(self.palette.colors):
            row = i // colors_per_row
            col = i % colors_per_row

            x = col * (self.color_size + self.spacing) + self.spacing
            y = row * (self.color_size + self.spacing) + self.spacing

            # Draw color
            painter.fillRect(x, y, self.color_size, self.color_size, color)

            # Draw border
            if i == self.selected_index:
                painter.setPen(QPen(QColor(theme.colors.accent_color), 3))
            else:
                painter.setPen(QPen(QColor(theme.colors.border), 1))
            painter.drawRect(x, y, self.color_size, self.color_size)

    def mousePressEvent(self, event):
        """Handle color selection"""
        colors_per_row = self.width() // (self.color_size + self.spacing)
        if colors_per_row == 0:
            return

        col = (event.pos().x() - self.spacing) // (self.color_size + self.spacing)
        row = (event.pos().y() - self.spacing) // (self.color_size + self.spacing)
        index = row * colors_per_row + col

        if 0 <= index < len(self.palette.colors):
            self.selected_index = index
            self.color_selected.emit(self.palette.colors[index])
            self.update()

    def add_color(self, color: QColor):
        """Add color to palette"""
        self.palette.add_color(color)
        self._update_size()
        self.update()

    def set_palette(self, palette: ColorPalette):
        """Set the palette"""
        self.palette = palette
        self._update_size()
        self.update()

    def _update_size(self):
        """Update widget size based on palette"""
        if not self.palette.colors:
            self.setMinimumHeight(100)
            self.setMaximumHeight(16777215)  # Reset max height
            return

        # Get available width, use minimum if not yet determined
        available_width = max(100, self.width())

        # Calculate how many rows we need
        colors_per_row = max(1, available_width // (self.color_size + self.spacing))
        num_rows = (len(self.palette.colors) + colors_per_row - 1) // colors_per_row

        # Set fixed height based on content
        height = num_rows * (self.color_size + self.spacing) + self.spacing + 10
        self.setMinimumHeight(height)
        self.setMaximumHeight(height)
        self.updateGeometry()


class TilePreviewWidget(QWidget):
    """Widget for previewing tiles in repetition"""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.canvas = None
        self.tile_x = 0
        self.tile_y = 0
        self.tile_width = 8
        self.tile_height = 8
        self.repeat_x = 3
        self.repeat_y = 3
        self.zoom = 4.0
        self.show_paint_mode = False
        self.painted_tiles = {}  # Dict of (tx, ty) -> tile_index for painted pattern

        self.setMinimumSize(200, 200)

        # Set background
        theme_manager = get_theme_manager()
        theme = theme_manager.get_current_theme()
        palette = self.palette()
        palette.setColor(QPalette.ColorRole.Window, QColor(theme.colors.surface))
        self.setPalette(palette)
        self.setAutoFillBackground(True)

    def set_canvas(self, canvas: 'PixelCanvas'):
        """Set the canvas to preview from"""
        self.canvas = canvas
        self.update()

    def set_tile_size(self, width: int, height: int):
        """Set the tile size"""
        self.tile_width = width
        self.tile_height = height
        self.update()

    def set_tile_position(self, x: int, y: int):
        """Set which tile to preview (in tile coordinates)"""
        self.tile_x = x
        self.tile_y = y
        self.update()

    def set_repeat(self, x: int, y: int):
        """Set how many times to repeat the tile"""
        self.repeat_x = max(1, x)
        self.repeat_y = max(1, y)
        self.update()

    def paintEvent(self, event):
        """Paint the tiled preview"""
        if not self.canvas:
            return

        theme_manager = get_theme_manager()
        theme = theme_manager.get_current_theme()
        c = theme.colors

        painter = QPainter(self)
        painter.fillRect(self.rect(), QColor(c.surface))

        # Get composite from canvas
        composite = self.canvas.composite_layers()

        # Calculate preview size
        preview_width = int(self.tile_width * self.repeat_x * self.zoom)
        preview_height = int(self.tile_height * self.repeat_y * self.zoom)

        # Center the preview
        offset_x = (self.width() - preview_width) // 2
        offset_y = (self.height() - preview_height) // 2

        # Draw checkerboard
        self._draw_checkerboard(painter, offset_x, offset_y, preview_width, preview_height)

        # Extract the tile from canvas
        src_x = self.tile_x * self.tile_width
        src_y = self.tile_y * self.tile_height

        # Make sure tile is within bounds
        if src_x + self.tile_width > self.canvas.canvas_width:
            src_x = self.canvas.canvas_width - self.tile_width
        if src_y + self.tile_height > self.canvas.canvas_height:
            src_y = self.canvas.canvas_height - self.tile_height

        src_x = max(0, src_x)
        src_y = max(0, src_y)

        # Draw tiled pattern
        for ty in range(self.repeat_y):
            for tx in range(self.repeat_x):
                dest_x = offset_x + int(tx * self.tile_width * self.zoom)
                dest_y = offset_y + int(ty * self.tile_height * self.zoom)

                # Check if this tile has been painted with a different tile
                tile_key = (tx, ty)
                if self.show_paint_mode and tile_key in self.painted_tiles:
                    paint_tile_x, paint_tile_y = self.painted_tiles[tile_key]
                    paint_src_x = paint_tile_x * self.tile_width
                    paint_src_y = paint_tile_y * self.tile_height

                    # Extract painted tile
                    tile = composite.copy(paint_src_x, paint_src_y, self.tile_width, self.tile_height)
                else:
                    # Extract tile from source position
                    tile = composite.copy(src_x, src_y, self.tile_width, self.tile_height)

                # Scale and draw
                scaled_tile = tile.scaled(
                    int(self.tile_width * self.zoom),
                    int(self.tile_height * self.zoom),
                    Qt.AspectRatioMode.KeepAspectRatio,
                    Qt.TransformationMode.FastTransformation
                )
                painter.drawPixmap(dest_x, dest_y, scaled_tile)

        # Draw grid lines
        grid_color = QColor(c.accent_color)
        grid_color.setAlpha(128)
        painter.setPen(QPen(grid_color, 1))

        for i in range(self.repeat_x + 1):
            x = offset_x + int(i * self.tile_width * self.zoom)
            painter.drawLine(x, offset_y, x, offset_y + preview_height)

        for i in range(self.repeat_y + 1):
            y = offset_y + int(i * self.tile_height * self.zoom)
            painter.drawLine(offset_x, y, offset_x + preview_width, y)

        # Draw border
        painter.setPen(QPen(QColor(c.border), 2))
        painter.drawRect(offset_x, offset_y, preview_width, preview_height)

    def _draw_checkerboard(self, painter: QPainter, x: int, y: int, width: int, height: int):
        """Draw checkerboard pattern"""
        checker_size = 10
        theme_manager = get_theme_manager()
        theme = theme_manager.get_current_theme()

        light_color = QColor(theme.colors.surface_hover)
        dark_color = QColor(theme.colors.surface)

        cols = (width // checker_size) + 1
        rows = (height // checker_size) + 1

        for row in range(rows):
            for col in range(cols):
                color = light_color if (row + col) % 2 == 0 else dark_color
                painter.fillRect(x + col * checker_size, y + row * checker_size,
                               checker_size, checker_size, color)

    def mousePressEvent(self, event):
        """Handle tile painting in preview"""
        if not self.show_paint_mode or not self.canvas:
            return

        # Calculate which tile was clicked
        preview_width = int(self.tile_width * self.repeat_x * self.zoom)
        preview_height = int(self.tile_height * self.repeat_y * self.zoom)
        offset_x = (self.width() - preview_width) // 2
        offset_y = (self.height() - preview_height) // 2

        rel_x = event.pos().x() - offset_x
        rel_y = event.pos().y() - offset_y

        if 0 <= rel_x < preview_width and 0 <= rel_y < preview_height:
            tile_x = int(rel_x / (self.tile_width * self.zoom))
            tile_y = int(rel_y / (self.tile_height * self.zoom))

            # Paint this tile with the current tile
            self.painted_tiles[(tile_x, tile_y)] = (self.tile_x, self.tile_y)
            self.update()

    def clear_painted_tiles(self):
        """Clear all painted tiles"""
        self.painted_tiles.clear()
        self.update()


class TilePreviewDialog(QDialog):
    """Dialog for tile preview"""

    def __init__(self, canvas: 'PixelCanvas', parent=None):
        super().__init__(parent)
        self.setWindowTitle("Tile Preview")
        self.setModal(False)
        self.canvas = canvas

        layout = QVBoxLayout()

        # Preview widget
        self.preview = TilePreviewWidget()
        self.preview.set_canvas(canvas)
        self.preview.setMinimumSize(400, 400)
        layout.addWidget(self.preview)

        # Controls
        controls_group = QGroupBox("Preview Settings")
        controls_layout = QFormLayout()

        # Tile size (read from canvas)
        self.tile_width_spin = QSpinBox()
        self.tile_width_spin.setMinimum(1)
        self.tile_width_spin.setMaximum(256)
        self.tile_width_spin.setValue(8)
        self.tile_width_spin.valueChanged.connect(self._update_tile_size)
        controls_layout.addRow("Tile Width:", self.tile_width_spin)

        self.tile_height_spin = QSpinBox()
        self.tile_height_spin.setMinimum(1)
        self.tile_height_spin.setMaximum(256)
        self.tile_height_spin.setValue(8)
        self.tile_height_spin.valueChanged.connect(self._update_tile_size)
        controls_layout.addRow("Tile Height:", self.tile_height_spin)

        # Repeat counts
        self.repeat_x_spin = QSpinBox()
        self.repeat_x_spin.setMinimum(1)
        self.repeat_x_spin.setMaximum(10)
        self.repeat_x_spin.setValue(3)
        self.repeat_x_spin.valueChanged.connect(self._update_repeat)
        controls_layout.addRow("Repeat X:", self.repeat_x_spin)

        self.repeat_y_spin = QSpinBox()
        self.repeat_y_spin.setMinimum(1)
        self.repeat_y_spin.setMaximum(10)
        self.repeat_y_spin.setValue(3)
        self.repeat_y_spin.valueChanged.connect(self._update_repeat)
        controls_layout.addRow("Repeat Y:", self.repeat_y_spin)

        # Tile selection
        self.tile_x_spin = QSpinBox()
        self.tile_x_spin.setMinimum(0)
        self.tile_x_spin.setMaximum(100)
        self.tile_x_spin.setValue(0)
        self.tile_x_spin.valueChanged.connect(self._update_tile_pos)
        controls_layout.addRow("Tile X:", self.tile_x_spin)

        self.tile_y_spin = QSpinBox()
        self.tile_y_spin.setMinimum(0)
        self.tile_y_spin.setMaximum(100)
        self.tile_y_spin.setValue(0)
        self.tile_y_spin.valueChanged.connect(self._update_tile_pos)
        controls_layout.addRow("Tile Y:", self.tile_y_spin)

        # Paint mode checkbox
        self.paint_mode_check = QCheckBox("Paint Mode (click to place tiles)")
        self.paint_mode_check.stateChanged.connect(self._toggle_paint_mode)
        controls_layout.addRow("", self.paint_mode_check)

        # Clear button
        clear_btn = QPushButton("Clear Painted Tiles")
        clear_btn.clicked.connect(self.preview.clear_painted_tiles)
        controls_layout.addRow("", clear_btn)

        controls_group.setLayout(controls_layout)
        layout.addWidget(controls_group)

        self.setLayout(layout)
        self.resize(500, 700)

    def _update_tile_size(self):
        """Update tile size in preview"""
        self.preview.set_tile_size(
            self.tile_width_spin.value(),
            self.tile_height_spin.value()
        )

    def _update_repeat(self):
        """Update repeat counts"""
        self.preview.set_repeat(
            self.repeat_x_spin.value(),
            self.repeat_y_spin.value()
        )

    def _update_tile_pos(self):
        """Update tile position"""
        self.preview.set_tile_position(
            self.tile_x_spin.value(),
            self.tile_y_spin.value()
        )

    def _toggle_paint_mode(self, state):
        """Toggle paint mode"""
        self.preview.show_paint_mode = (state == Qt.CheckState.Checked.value)

    def refresh_preview(self):
        """Refresh the preview (call this when canvas changes)"""
        self.preview.update()


class NewCanvasDialog(QDialog):
    """Dialog for creating a new canvas"""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("New Canvas")
        self.setModal(True)
        self.setMinimumWidth(300)

        layout = QVBoxLayout()

        form_layout = QFormLayout()

        # Width
        self.width_spin = QSpinBox()
        self.width_spin.setMinimum(1)
        self.width_spin.setMaximum(4096)
        self.width_spin.setValue(32)
        self.width_spin.setSuffix(" px")
        form_layout.addRow("Width:", self.width_spin)

        # Height
        self.height_spin = QSpinBox()
        self.height_spin.setMinimum(1)
        self.height_spin.setMaximum(4096)
        self.height_spin.setValue(32)
        self.height_spin.setSuffix(" px")
        form_layout.addRow("Height:", self.height_spin)

        layout.addLayout(form_layout)

        # Buttons
        buttons = QDialogButtonBox(
            QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel
        )
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)

        self.setLayout(layout)

    def get_canvas_data(self):
        """Get canvas data"""
        return {
            'width': self.width_spin.value(),
            'height': self.height_spin.value()
        }


# ============================================================================
# Animation Preview Widget
# ============================================================================

class AnimationPreviewWidget(QWidget):
    """Widget for previewing animations with playback controls"""

    def __init__(self, canvas_widget, parent=None):
        super().__init__(parent)
        self.canvas_widget = canvas_widget
        self.current_frame = 0
        self.is_playing = False
        self.playback_timer = QTimer()
        self.playback_timer.timeout.connect(self._advance_frame)
        self.fps = 10
        self.selected_tag = None  # None = all frames

        # Get theme
        theme_manager = get_theme_manager()
        theme = theme_manager.get_current_theme()
        c = theme.colors

        self.setWindowTitle("Animation Preview")
        self.setMinimumSize(400, 300)

        layout = QVBoxLayout()

        # Preview area
        self.preview_label = QLabel()
        self.preview_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.preview_label.setStyleSheet(f"background-color: {c.surface}; border: 1px solid {c.border};")
        self.preview_label.setMinimumSize(200, 200)
        layout.addWidget(self.preview_label, 1)

        # Controls
        controls = QHBoxLayout()

        self.play_btn = QPushButton("Play")
        self.play_btn.clicked.connect(self._toggle_playback)
        controls.addWidget(self.play_btn)

        self.stop_btn = QPushButton("Stop")
        self.stop_btn.clicked.connect(self._stop_playback)
        controls.addWidget(self.stop_btn)

        self.prev_btn = QPushButton("< Prev")
        self.prev_btn.clicked.connect(self._prev_frame)
        controls.addWidget(self.prev_btn)

        self.next_btn = QPushButton("Next >")
        self.next_btn.clicked.connect(self._next_frame)
        controls.addWidget(self.next_btn)

        controls.addStretch()

        # FPS control
        controls.addWidget(QLabel("FPS:"))
        self.fps_spin = QSpinBox()
        self.fps_spin.setMinimum(1)
        self.fps_spin.setMaximum(60)
        self.fps_spin.setValue(10)
        self.fps_spin.valueChanged.connect(self._on_fps_changed)
        controls.addWidget(self.fps_spin)

        layout.addLayout(controls)

        # Animation tag selector
        tag_layout = QHBoxLayout()
        tag_layout.addWidget(QLabel("Animation:"))

        self.tag_combo = QComboBox()
        self.tag_combo.addItem("All Frames", None)
        self.tag_combo.currentIndexChanged.connect(self._on_tag_changed)
        tag_layout.addWidget(self.tag_combo, 1)

        self.manage_tags_btn = QPushButton("Manage Tags...")
        self.manage_tags_btn.clicked.connect(self._open_tag_manager)
        tag_layout.addWidget(self.manage_tags_btn)

        layout.addLayout(tag_layout)

        # Frame info
        self.frame_info = QLabel("Frame: 1/1")
        self.frame_info.setStyleSheet(f"color: {c.text_secondary};")
        layout.addWidget(self.frame_info)

        self.setLayout(layout)

        self._update_tag_list()
        self._update_preview()

    def _update_tag_list(self):
        """Update the list of animation tags"""
        current_text = self.tag_combo.currentText()
        self.tag_combo.clear()
        self.tag_combo.addItem("All Frames", None)

        for tag in self.canvas_widget.animation_tags:
            self.tag_combo.addItem(tag.name, tag)

        # Try to restore selection
        index = self.tag_combo.findText(current_text)
        if index >= 0:
            self.tag_combo.setCurrentIndex(index)

    def _on_tag_changed(self, index):
        """Handle animation tag selection change"""
        self.selected_tag = self.tag_combo.itemData(index)
        self.current_frame = 0
        self._update_preview()

    def _get_frame_range(self):
        """Get the frame range for current selection"""
        if self.selected_tag:
            return self.selected_tag.start_frame, self.selected_tag.end_frame
        else:
            return 0, self.canvas_widget.get_frame_count() - 1

    def _toggle_playback(self):
        """Toggle play/pause"""
        if self.is_playing:
            self.is_playing = False
            self.playback_timer.stop()
            self.play_btn.setText("Play")
        else:
            self.is_playing = True
            self.playback_timer.start(int(1000 / self.fps))
            self.play_btn.setText("Pause")

    def _stop_playback(self):
        """Stop playback"""
        self.is_playing = False
        self.playback_timer.stop()
        self.play_btn.setText("Play")
        self.current_frame = 0
        self._update_preview()

    def _prev_frame(self):
        """Go to previous frame"""
        start, end = self._get_frame_range()
        if self.current_frame > 0:
            self.current_frame -= 1
        else:
            self.current_frame = end - start
        self._update_preview()

    def _next_frame(self):
        """Go to next frame"""
        start, end = self._get_frame_range()
        if self.current_frame < end - start:
            self.current_frame += 1
        else:
            self.current_frame = 0
        self._update_preview()

    def _advance_frame(self):
        """Advance to next frame (for playback)"""
        start, end = self._get_frame_range()

        if self.selected_tag and self.selected_tag.direction == "reverse":
            if self.current_frame > 0:
                self.current_frame -= 1
            else:
                self.current_frame = end - start
        elif self.selected_tag and self.selected_tag.direction == "pingpong":
            # TODO: Implement pingpong
            if self.current_frame < end - start:
                self.current_frame += 1
            else:
                self.current_frame = 0
        else:  # forward
            if self.current_frame < end - start:
                self.current_frame += 1
            else:
                self.current_frame = 0

        self._update_preview()

    def _on_fps_changed(self, value):
        """Handle FPS change"""
        self.fps = value
        if self.is_playing:
            self.playback_timer.setInterval(int(1000 / self.fps))

    def _update_preview(self):
        """Update the preview display"""
        start, end = self._get_frame_range()
        actual_frame = start + self.current_frame

        # Composite the frame
        frame_pixmap = self.canvas_widget._composite_frame(actual_frame)

        # Scale to fit preview
        scaled = frame_pixmap.scaled(
            self.preview_label.size(),
            Qt.AspectRatioMode.KeepAspectRatio,
            Qt.TransformationMode.FastTransformation
        )

        self.preview_label.setPixmap(scaled)

        # Update frame info
        self.frame_info.setText(f"Frame: {actual_frame + 1}/{self.canvas_widget.get_frame_count()}")

    def _open_tag_manager(self):
        """Open animation tag manager"""
        dialog = AnimationTagManagerDialog(self.canvas_widget, self)
        if dialog.exec() == QDialog.DialogCode.Accepted:
            self._update_tag_list()

    def resizeEvent(self, event):
        """Handle resize"""
        super().resizeEvent(event)
        self._update_preview()


# ============================================================================
# Animation Tag Manager Dialog
# ============================================================================

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


# ============================================================================
# Sprite Sheet Export Dialog
# ============================================================================

class SpriteSheetExportDialog(QDialog):
    """Dialog for sprite sheet export with advanced options"""

    def __init__(self, canvas_widget, parent=None):
        super().__init__(parent)
        self.canvas_widget = canvas_widget

        # Get theme
        theme_manager = get_theme_manager()
        theme = theme_manager.get_current_theme()
        c = theme.colors

        self.setWindowTitle("Export Sprite Sheet")
        self.setMinimumWidth(400)

        layout = QVBoxLayout()

        # Layout options
        layout_group = QGroupBox("Layout")
        layout_layout = QVBoxLayout()

        self.layout_button_group = QButtonGroup()

        self.horizontal_radio = QRadioButton("Horizontal Strip")
        self.horizontal_radio.setChecked(True)
        self.layout_button_group.addButton(self.horizontal_radio, 0)
        layout_layout.addWidget(self.horizontal_radio)

        self.vertical_radio = QRadioButton("Vertical Strip")
        self.layout_button_group.addButton(self.vertical_radio, 1)
        layout_layout.addWidget(self.vertical_radio)

        self.grid_radio = QRadioButton("Grid")
        self.layout_button_group.addButton(self.grid_radio, 2)
        layout_layout.addWidget(self.grid_radio)

        self.per_tag_radio = QRadioButton("One Row Per Animation Tag")
        self.layout_button_group.addButton(self.per_tag_radio, 3)
        layout_layout.addWidget(self.per_tag_radio)

        # Grid dimensions
        grid_layout = QHBoxLayout()
        grid_layout.addWidget(QLabel("Grid Columns:"))
        self.columns_spin = QSpinBox()
        self.columns_spin.setMinimum(1)
        self.columns_spin.setMaximum(100)
        self.columns_spin.setValue(8)
        grid_layout.addWidget(self.columns_spin)
        grid_layout.addStretch()
        layout_layout.addLayout(grid_layout)

        layout_group.setLayout(layout_layout)
        layout.addWidget(layout_group)

        # Options
        options_group = QGroupBox("Options")
        options_layout = QVBoxLayout()

        self.skip_duplicates_check = QCheckBox("Skip Duplicate Frames")
        options_layout.addWidget(self.skip_duplicates_check)

        self.export_json_check = QCheckBox("Export JSON Metadata")
        options_layout.addWidget(self.export_json_check)

        options_group.setLayout(options_layout)
        layout.addWidget(options_group)

        # Dialog buttons
        buttons = QDialogButtonBox(
            QDialogButtonBox.StandardButton.Ok |
            QDialogButtonBox.StandardButton.Cancel
        )
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)

        self.setLayout(layout)

    def get_options(self):
        """Get export options"""
        layout_type = self.layout_button_group.checkedId()
        layout_names = ["horizontal", "vertical", "grid", "per_tag"]

        return {
            'layout': layout_names[layout_type],
            'columns': self.columns_spin.value(),
            'skip_duplicates': self.skip_duplicates_check.isChecked(),
            'export_json': self.export_json_check.isChecked()
        }


class PixelPainterTool(EditorPanel):
    """Pixel Painter tool for creating pixel art"""

    def __init__(self, parent=None, project_root=None):
        self.project_root = project_root
        self.current_file = None
        self.current_palette = ColorPalette()
        self.tile_preview_dialog = None  # Tile preview window

        super().__init__("Pixel Painter", parent)
        self.setObjectName("PixelPainterTool")

        self.setFocusPolicy(Qt.FocusPolicy.StrongFocus)

        # Create default canvas
        self._create_default_canvas()

        # Setup shortcuts
        QTimer.singleShot(0, self._setup_shortcuts)

        self.setFocus()
        self.installEventFilter(self)

    def showEvent(self, event):
        """Override showEvent"""
        super().showEvent(event)
        self.setFocus()
        self.activateWindow()

    def _setup_panel(self):
        """Setup pixel painter panel UI"""
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
        self.canvas_widget = None
        canvas_scroll = QScrollArea()
        canvas_scroll.setWidgetResizable(True)
        canvas_scroll.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.canvas_scroll = canvas_scroll
        main_splitter.addWidget(canvas_scroll)

        # Right side - Color and palette
        right_widget = self._create_right_panel()
        main_splitter.addWidget(right_widget)

        main_splitter.setSizes([200, 600, 250])
        main_layout.addWidget(main_splitter, 1)

        # Timeline (hidden by default)
        self.timeline_widget = TimelineWidget()
        self.timeline_widget.setVisible(False)
        self.timeline_widget.frame_selected.connect(self._on_timeline_frame_selected)
        self.timeline_widget.new_frame_btn.clicked.connect(self._new_frame)
        self.timeline_widget.duplicate_frame_btn.clicked.connect(self._duplicate_frame)
        self.timeline_widget.delete_frame_btn.clicked.connect(self._delete_frame)
        self.timeline_widget.move_left_btn.clicked.connect(self._move_frame_left)
        self.timeline_widget.move_right_btn.clicked.connect(self._move_frame_right)
        main_layout.addWidget(self.timeline_widget)

        main_widget.setLayout(main_layout)
        self.content_layout.addWidget(main_widget)

    def _create_menubar(self) -> QMenuBar:
        """Create menu bar"""
        menubar = QMenuBar()

        # File menu
        file_menu = menubar.addMenu("File")

        new_action = QAction("New", self)
        new_action.setShortcut(QKeySequence.StandardKey.New)
        new_action.triggered.connect(self._new_canvas)
        file_menu.addAction(new_action)

        open_action = QAction("Open", self)
        open_action.setShortcut(QKeySequence.StandardKey.Open)
        open_action.triggered.connect(self._open_file)
        file_menu.addAction(open_action)

        file_menu.addSeparator()

        # Save (Ctrl+S handled centrally by the main editor via handle_save to
        # avoid an ambiguous shortcut overload with the editor's save action)
        save_action = QAction("Save", self)
        save_action.triggered.connect(self._save_file)
        file_menu.addAction(save_action)

        save_as_action = QAction("Save As...", self)
        save_as_action.triggered.connect(self._save_as_file)
        file_menu.addAction(save_as_action)

        file_menu.addSeparator()

        export_action = QAction("Export PNG...", self)
        export_action.setShortcut(QKeySequence("Ctrl+E"))
        export_action.triggered.connect(self._export_png)
        file_menu.addAction(export_action)

        # Edit menu
        edit_menu = menubar.addMenu("Edit")

        self.undo_action = QAction("Undo", self)
        self.undo_action.triggered.connect(self._undo)
        self.undo_action.setEnabled(False)
        edit_menu.addAction(self.undo_action)

        self.redo_action = QAction("Redo", self)
        self.redo_action.triggered.connect(self._redo)
        self.redo_action.setEnabled(False)
        edit_menu.addAction(self.redo_action)

        # View menu
        view_menu = menubar.addMenu("View")

        reset_view_action = QAction("Reset View", self)
        reset_view_action.setShortcut(QKeySequence("Ctrl+0"))
        reset_view_action.triggered.connect(self._reset_view)
        view_menu.addAction(reset_view_action)

        self.grid_action = QAction("Show Pixel Grid", self)
        self.grid_action.setCheckable(True)
        self.grid_action.setChecked(True)
        self.grid_action.triggered.connect(self._toggle_grid)
        view_menu.addAction(self.grid_action)

        zoom_in_action = QAction("Zoom In", self)
        zoom_in_action.setShortcut(QKeySequence("Ctrl++"))
        zoom_in_action.triggered.connect(self._zoom_in)
        view_menu.addAction(zoom_in_action)

        zoom_out_action = QAction("Zoom Out", self)
        zoom_out_action.setShortcut(QKeySequence("Ctrl+-"))
        zoom_out_action.triggered.connect(self._zoom_out)
        view_menu.addAction(zoom_out_action)

        view_menu.addSeparator()

        tile_preview_action = QAction("Tile Preview...", self)
        tile_preview_action.setShortcut(QKeySequence("Ctrl+T"))
        tile_preview_action.triggered.connect(self._open_tile_preview)
        view_menu.addAction(tile_preview_action)

        # Animation menu
        animation_menu = menubar.addMenu("Animation")

        self.timeline_action = QAction("Show Timeline", self)
        self.timeline_action.setCheckable(True)
        self.timeline_action.setChecked(False)
        self.timeline_action.triggered.connect(self._toggle_timeline)
        animation_menu.addAction(self.timeline_action)

        self.onion_skin_action = QAction("Onion Skinning", self)
        self.onion_skin_action.setCheckable(True)
        self.onion_skin_action.setChecked(False)
        self.onion_skin_action.triggered.connect(self._toggle_onion_skinning)
        animation_menu.addAction(self.onion_skin_action)

        animation_menu.addSeparator()

        animation_preview_action = QAction("Animation Preview...", self)
        animation_preview_action.setShortcut(QKeySequence("Ctrl+P"))
        animation_preview_action.triggered.connect(self._open_animation_preview)
        animation_menu.addAction(animation_preview_action)

        manage_tags_action = QAction("Manage Animation Tags...", self)
        manage_tags_action.triggered.connect(self._open_tag_manager)
        animation_menu.addAction(manage_tags_action)

        animation_menu.addSeparator()

        export_spritesheet_action = QAction("Export Sprite Sheet...", self)
        export_spritesheet_action.triggered.connect(self._export_spritesheet)
        animation_menu.addAction(export_spritesheet_action)

        export_gif_action = QAction("Export GIF...", self)
        export_gif_action.triggered.connect(self._export_gif)
        animation_menu.addAction(export_gif_action)

        export_video_action = QAction("Export Video (MP4/MOV)...", self)
        export_video_action.triggered.connect(self._export_video)
        animation_menu.addAction(export_video_action)

        # Palette menu
        palette_menu = menubar.addMenu("Palette")

        load_palette_action = QAction("Load Palette...", self)
        load_palette_action.triggered.connect(self._load_palette)
        palette_menu.addAction(load_palette_action)

        save_palette_action = QAction("Save Palette...", self)
        save_palette_action.triggered.connect(self._save_palette)
        palette_menu.addAction(save_palette_action)

        return menubar

    def _create_toolbar(self) -> QWidget:
        """Create toolbar"""
        toolbar = QWidget()
        layout = QHBoxLayout()
        layout.setContentsMargins(0, 0, 0, 0)

        layout.addStretch()

        toolbar.setLayout(layout)
        return toolbar

    def _create_left_panel(self) -> QWidget:
        """Create left panel with tools and layers"""
        widget = QWidget()
        layout = QVBoxLayout()
        layout.setContentsMargins(0, 0, 0, 0)

        # Tools group
        tools_group = QGroupBox("Tools")
        tools_layout = QVBoxLayout()

        theme_manager = get_theme_manager()
        theme = theme_manager.get_current_theme()
        c = theme.colors

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

        self.tool_pen = QRadioButton("Pen")
        self.tool_pen.setStyleSheet(radio_style)
        self.tool_pen.setChecked(True)
        self.tool_pen.toggled.connect(lambda: self._set_tool("pen"))
        tools_layout.addWidget(self.tool_pen)

        self.tool_eraser = QRadioButton("Eraser")
        self.tool_eraser.setStyleSheet(radio_style)
        self.tool_eraser.toggled.connect(lambda: self._set_tool("eraser"))
        tools_layout.addWidget(self.tool_eraser)

        self.tool_bucket = QRadioButton("Bucket Fill")
        self.tool_bucket.setStyleSheet(radio_style)
        self.tool_bucket.toggled.connect(lambda: self._set_tool("bucket"))
        tools_layout.addWidget(self.tool_bucket)

        self.tool_eyedropper = QRadioButton("Eyedropper")
        self.tool_eyedropper.setStyleSheet(radio_style)
        self.tool_eyedropper.toggled.connect(lambda: self._set_tool("eyedropper"))
        tools_layout.addWidget(self.tool_eyedropper)

        # Pen size
        size_layout = QHBoxLayout()
        size_layout.addWidget(QLabel("Size:"))
        self.pen_size_spin = QSpinBox()
        self.pen_size_spin.setMinimum(1)
        self.pen_size_spin.setMaximum(32)
        self.pen_size_spin.setValue(1)
        self.pen_size_spin.valueChanged.connect(self._on_pen_size_changed)
        size_layout.addWidget(self.pen_size_spin)
        tools_layout.addLayout(size_layout)

        tools_group.setLayout(tools_layout)
        layout.addWidget(tools_group)

        # Grid group
        grid_group = QGroupBox("Grid Settings")
        grid_layout = QVBoxLayout()

        # Tile grid checkbox
        self.tile_grid_check = QCheckBox("Show Tile Grid")
        self.tile_grid_check.setChecked(False)
        self.tile_grid_check.stateChanged.connect(self._toggle_tile_grid)
        grid_layout.addWidget(self.tile_grid_check)

        # Tile grid size
        tile_grid_size_layout = QHBoxLayout()
        tile_grid_size_layout.addWidget(QLabel("W:"))
        self.tile_grid_width_spin = QSpinBox()
        self.tile_grid_width_spin.setMinimum(1)
        self.tile_grid_width_spin.setMaximum(256)
        self.tile_grid_width_spin.setValue(8)
        self.tile_grid_width_spin.valueChanged.connect(self._on_tile_grid_size_changed)
        tile_grid_size_layout.addWidget(self.tile_grid_width_spin)

        tile_grid_size_layout.addWidget(QLabel("H:"))
        self.tile_grid_height_spin = QSpinBox()
        self.tile_grid_height_spin.setMinimum(1)
        self.tile_grid_height_spin.setMaximum(256)
        self.tile_grid_height_spin.setValue(8)
        self.tile_grid_height_spin.valueChanged.connect(self._on_tile_grid_size_changed)
        tile_grid_size_layout.addWidget(self.tile_grid_height_spin)
        grid_layout.addLayout(tile_grid_size_layout)

        grid_group.setLayout(grid_layout)
        layout.addWidget(grid_group)

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

        duplicate_layer_btn = QPushButton("Dup")
        duplicate_layer_btn.clicked.connect(self._duplicate_layer)
        layer_buttons.addWidget(duplicate_layer_btn)

        layers_layout.addLayout(layer_buttons)

        self.layer_list = QListWidget()
        self.layer_list.currentRowChanged.connect(self._on_layer_selected)
        layers_layout.addWidget(self.layer_list)

        # Layer properties section
        props_frame = QFrame()
        props_frame.setFrameStyle(QFrame.Shape.StyledPanel | QFrame.Shadow.Raised)
        props_layout = QFormLayout()
        props_layout.setContentsMargins(4, 4, 4, 4)
        props_layout.setSpacing(4)

        # Layer name
        self.layer_name_edit = QLineEdit()
        self.layer_name_edit.editingFinished.connect(self._on_layer_name_changed)
        props_layout.addRow("Name:", self.layer_name_edit)

        # Opacity slider
        opacity_layout = QHBoxLayout()
        self.layer_opacity_slider = QSlider(Qt.Orientation.Horizontal)
        self.layer_opacity_slider.setMinimum(0)
        self.layer_opacity_slider.setMaximum(100)
        self.layer_opacity_slider.setValue(100)
        self.layer_opacity_slider.valueChanged.connect(self._on_layer_opacity_changed)
        opacity_layout.addWidget(self.layer_opacity_slider)

        self.layer_opacity_label = QLabel("100%")
        self.layer_opacity_label.setMinimumWidth(40)
        opacity_layout.addWidget(self.layer_opacity_label)
        props_layout.addRow("Opacity:", opacity_layout)

        # Blend mode
        self.layer_blend_combo = QComboBox()
        self.layer_blend_combo.addItems(BlendMode.get_all())
        self.layer_blend_combo.currentTextChanged.connect(self._on_layer_blend_changed)
        props_layout.addRow("Blend:", self.layer_blend_combo)

        # Checkboxes
        checks_layout = QGridLayout()
        checks_layout.setSpacing(2)

        self.layer_visible_check = QCheckBox("Visible")
        self.layer_visible_check.stateChanged.connect(self._on_layer_visible_changed)
        checks_layout.addWidget(self.layer_visible_check, 0, 0)

        self.layer_locked_check = QCheckBox("Locked")
        self.layer_locked_check.stateChanged.connect(self._on_layer_locked_changed)
        checks_layout.addWidget(self.layer_locked_check, 0, 1)

        self.layer_alpha_lock_check = QCheckBox("Alpha Lock")
        self.layer_alpha_lock_check.stateChanged.connect(self._on_layer_alpha_lock_changed)
        checks_layout.addWidget(self.layer_alpha_lock_check, 1, 0)

        self.layer_clipping_check = QCheckBox("Clipping")
        self.layer_clipping_check.stateChanged.connect(self._on_layer_clipping_changed)
        checks_layout.addWidget(self.layer_clipping_check, 1, 1)

        props_layout.addRow("", checks_layout)

        props_frame.setLayout(props_layout)
        layers_layout.addWidget(props_frame)

        layers_group.setLayout(layers_layout)
        layout.addWidget(layers_group)

        widget.setLayout(layout)
        return widget

    def _create_right_panel(self) -> QWidget:
        """Create right panel with color and palette"""
        widget = QWidget()
        layout = QVBoxLayout()
        layout.setContentsMargins(0, 0, 0, 0)

        # Current color
        color_group = QGroupBox("Current Color")
        color_layout = QVBoxLayout()

        self.color_display = QLabel()
        self.color_display.setMinimumHeight(50)
        self.color_display.setStyleSheet("border: 1px solid #ccc;")
        self._update_color_display()
        color_layout.addWidget(self.color_display)

        pick_color_btn = QPushButton("Pick Color...")
        pick_color_btn.clicked.connect(self._pick_color_dialog)
        color_layout.addWidget(pick_color_btn)

        add_to_palette_btn = QPushButton("Add to Palette")
        add_to_palette_btn.clicked.connect(self._add_current_color_to_palette)
        color_layout.addWidget(add_to_palette_btn)

        color_group.setLayout(color_layout)
        layout.addWidget(color_group)

        # Color wheel
        wheel_group = QGroupBox("Color Wheel")
        wheel_layout = QVBoxLayout()

        self.color_wheel = ColorWheelWidget()
        self.color_wheel.color_changed.connect(self._on_color_changed)
        wheel_layout.addWidget(self.color_wheel)

        wheel_group.setLayout(wheel_layout)
        layout.addWidget(wheel_group)

        # Palette - takes remaining space with scroll
        palette_group = QGroupBox("Palette")
        palette_layout = QVBoxLayout()
        palette_layout.setContentsMargins(2, 2, 2, 2)

        # Create scroll area for palette
        palette_scroll = QScrollArea()
        palette_scroll.setWidgetResizable(True)
        palette_scroll.setHorizontalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAlwaysOff)
        palette_scroll.setVerticalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAsNeeded)
        palette_scroll.setFrameShape(QFrame.Shape.NoFrame)

        self.palette_widget = PaletteWidget()
        self.palette_widget.color_selected.connect(self._on_palette_color_selected)
        palette_scroll.setWidget(self.palette_widget)

        palette_layout.addWidget(palette_scroll)

        palette_group.setLayout(palette_layout)
        layout.addWidget(palette_group, 1)  # Stretch factor of 1 to take remaining space

        widget.setLayout(layout)
        return widget

    def _create_default_canvas(self):
        """Create default 32x32 canvas"""
        self.canvas_widget = PixelCanvas(32, 32)
        self.canvas_widget.canvas_modified.connect(self._on_canvas_modified)
        self.canvas_widget.history_changed.connect(self._on_history_changed)
        self.canvas_widget.color_picked.connect(self._on_color_picked)

        if hasattr(self, 'canvas_scroll'):
            self.canvas_scroll.setWidget(self.canvas_widget)
            self._update_layer_list()

        if hasattr(self, 'timeline_widget'):
            self.timeline_widget.set_canvas(self.canvas_widget)

    def _setup_shortcuts(self):
        """Setup keyboard shortcuts"""
        self.undo_shortcut = QShortcut(QKeySequence.StandardKey.Undo, self)
        self.undo_shortcut.activated.connect(self._undo)
        self.undo_shortcut.setContext(Qt.ShortcutContext.ApplicationShortcut)

        self.redo_shortcut = QShortcut(QKeySequence.StandardKey.Redo, self)
        self.redo_shortcut.activated.connect(self._redo)
        self.redo_shortcut.setContext(Qt.ShortcutContext.ApplicationShortcut)

        self.redo_shortcut_alt = QShortcut(QKeySequence("Ctrl+Y"), self)
        self.redo_shortcut_alt.activated.connect(self._redo)
        self.redo_shortcut_alt.setContext(Qt.ShortcutContext.ApplicationShortcut)

    def _new_canvas(self):
        """Create a new canvas"""
        dialog = NewCanvasDialog(self)
        if dialog.exec() == QDialog.DialogCode.Accepted:
            data = dialog.get_canvas_data()
            self.canvas_widget = PixelCanvas(data['width'], data['height'])
            self.canvas_widget.canvas_modified.connect(self._on_canvas_modified)
            self.canvas_widget.history_changed.connect(self._on_history_changed)
            self.canvas_widget.color_picked.connect(self._on_color_picked)
            self.canvas_scroll.setWidget(self.canvas_widget)
            self._update_layer_list()

            # Reset timeline
            self.timeline_widget.set_canvas(self.canvas_widget)
            self.timeline_widget.update_display()

            self.current_file = None

    def _open_file(self):
        """Open a file"""
        file_path, _ = QFileDialog.getOpenFileName(
            self,
            "Open File",
            str(self.project_root) if self.project_root else "",
            "Pixel Painter Files (*.pixart);;PNG Files (*.png);;All Files (*.*)"
        )

        if file_path:
            self._load_file(file_path)

    def _load_file(self, file_path: str):
        """Load a file"""
        path = Path(file_path)

        if path.suffix.lower() == '.pixart':
            # Load custom format
            try:
                with open(file_path, 'r') as f:
                    data = json.load(f)

                # Create new canvas
                width = data.get('width', 32)
                height = data.get('height', 32)
                self.canvas_widget = PixelCanvas(width, height)
                self.canvas_widget.canvas_modified.connect(self._on_canvas_modified)
                self.canvas_widget.history_changed.connect(self._on_history_changed)
                self.canvas_widget.color_picked.connect(self._on_color_picked)

                # Clear default layer
                self.canvas_widget.layers.clear()

                # Load layers
                import base64
                for layer_data in data.get('layers', []):
                    layer = Layer(layer_data['name'], width, height)
                    layer.visible = layer_data.get('visible', True)
                    layer.opacity = layer_data.get('opacity', 1.0)
                    layer.blend_mode = layer_data.get('blend_mode', BlendMode.NORMAL)
                    layer.locked = layer_data.get('locked', False)
                    layer.alpha_lock = layer_data.get('alpha_lock', False)
                    layer.clipping_mask = layer_data.get('clipping_mask', False)

                    # Load frames (new format) or single image (old format)
                    if 'frames' in layer_data:
                        layer.frames = []
                        for frame_data in layer_data['frames']:
                            frame = Frame(width, height)
                            image_data = base64.b64decode(frame_data['image'])
                            image = QImage()
                            image.loadFromData(image_data, 'PNG')
                            frame.canvas = QPixmap.fromImage(image)
                            frame.duration = frame_data.get('duration', 100)
                            layer.frames.append(frame)
                    elif 'image' in layer_data:
                        # Old format - single image
                        layer.frames = [Frame(width, height)]
                        image_data = base64.b64decode(layer_data['image'])
                        image = QImage()
                        image.loadFromData(image_data, 'PNG')
                        layer.frames[0].canvas = QPixmap.fromImage(image)

                    self.canvas_widget.layers.append(layer)

                if not self.canvas_widget.layers:
                    self.canvas_widget.add_layer("Layer 1")

                self.canvas_widget.current_layer_index = 0

                # Load animation data
                if 'animation_tags' in data:
                    self.canvas_widget.animation_tags = [AnimationTag.from_dict(tag) for tag in data['animation_tags']]

                if 'current_frame' in data:
                    self.canvas_widget.set_frame(data['current_frame'])

                # Load palette if present
                if 'palette' in data:
                    self.current_palette = ColorPalette.from_dict(data['palette'])
                    self.palette_widget.set_palette(self.current_palette)

                self.canvas_scroll.setWidget(self.canvas_widget)
                self._update_layer_list()

                # Update timeline
                self.timeline_widget.set_canvas(self.canvas_widget)
                self.timeline_widget.update_display()

                self.current_file = file_path

            except Exception as e:
                QMessageBox.critical(self, "Error", f"Failed to load file: {str(e)}")

        elif path.suffix.lower() in ['.png', '.jpg', '.jpeg']:
            # Load as image
            image = QImage(file_path)
            if not image.isNull():
                self.canvas_widget = PixelCanvas(image.width(), image.height())
                self.canvas_widget.canvas_modified.connect(self._on_canvas_modified)
                self.canvas_widget.history_changed.connect(self._on_history_changed)
                self.canvas_widget.color_picked.connect(self._on_color_picked)

                # Clear default layer and add image
                self.canvas_widget.layers[0].canvas = QPixmap.fromImage(image)

                self.canvas_scroll.setWidget(self.canvas_widget)
                self._update_layer_list()

                # Update timeline
                self.timeline_widget.set_canvas(self.canvas_widget)
                self.timeline_widget.update_display()

                self.current_file = None

    def handle_save(self) -> bool:
        """Save the current image when Ctrl+S is pressed with this tool focused"""
        self._save_file()
        return True

    def handle_save_as(self) -> bool:
        """Save the current image to a new path when Ctrl+Shift+S is pressed"""
        self._save_as_file()
        return True

    def _save_file(self):
        """Save file"""
        if self.current_file:
            self._save_to_file(self.current_file)
        else:
            self._save_as_file()

    def _save_as_file(self):
        """Save file as"""
        file_path, _ = QFileDialog.getSaveFileName(
            self,
            "Save File As",
            str(self.project_root) if self.project_root else "",
            "Pixel Painter Files (*.pixart)"
        )

        if file_path:
            if not file_path.endswith('.pixart'):
                file_path += '.pixart'
            self._save_to_file(file_path)

    def _save_to_file(self, file_path: str):
        """Save to file"""
        try:
            import base64

            data = {
                'width': self.canvas_widget.canvas_width,
                'height': self.canvas_widget.canvas_height,
                'layers': [],
                'palette': self.current_palette.to_dict()
            }

            # Save layers with animation frames
            for layer in self.canvas_widget.layers:
                frames_data = []

                # Save all frames for this layer
                for frame in layer.frames:
                    byte_array = QByteArray()
                    buffer = QBuffer(byte_array)
                    buffer.open(QBuffer.OpenModeFlag.WriteOnly)
                    frame.canvas.save(buffer, 'PNG')
                    image_data = base64.b64encode(byte_array.data()).decode('utf-8')

                    frame_data = {
                        'image': image_data,
                        'duration': frame.duration
                    }
                    frames_data.append(frame_data)

                layer_data = {
                    'name': layer.name,
                    'visible': layer.visible,
                    'opacity': layer.opacity,
                    'blend_mode': layer.blend_mode,
                    'locked': layer.locked,
                    'alpha_lock': layer.alpha_lock,
                    'clipping_mask': layer.clipping_mask,
                    'frames': frames_data
                }
                data['layers'].append(layer_data)

            # Save animation tags
            data['animation_tags'] = [tag.to_dict() for tag in self.canvas_widget.animation_tags]
            data['current_frame'] = self.canvas_widget.current_frame

            with open(file_path, 'w') as f:
                json.dump(data, f, indent=2)

            self.current_file = file_path
            QMessageBox.information(self, "Success", "File saved successfully!")

        except Exception as e:
            QMessageBox.critical(self, "Error", f"Failed to save file: {str(e)}")

    def _export_png(self):
        """Export as PNG"""
        file_path, _ = QFileDialog.getSaveFileName(
            self,
            "Export PNG",
            str(self.project_root) if self.project_root else "",
            "PNG Files (*.png)"
        )

        if file_path:
            if not file_path.endswith('.png'):
                file_path += '.png'

            if self.canvas_widget.save_to_file(file_path):
                QMessageBox.information(self, "Success", "PNG exported successfully!")
            else:
                QMessageBox.critical(self, "Error", "Failed to export PNG")

    def _load_palette(self):
        """Load a palette file"""
        file_path, _ = QFileDialog.getOpenFileName(
            self,
            "Load Palette",
            str(self.project_root) if self.project_root else "",
            "Palette Files (*.gpl *.pal);;All Files (*.*)"
        )

        if file_path:
            try:
                palette = ColorPalette.load_aseprite_palette(file_path)
                self.current_palette = palette
                self.palette_widget.set_palette(palette)
                QMessageBox.information(self, "Success", f"Loaded palette: {palette.name}")
            except Exception as e:
                QMessageBox.critical(self, "Error", f"Failed to load palette: {str(e)}")

    def _save_palette(self):
        """Save current palette"""
        file_path, _ = QFileDialog.getSaveFileName(
            self,
            "Save Palette",
            str(self.project_root) if self.project_root else "",
            "GIMP Palette (*.gpl)"
        )

        if file_path:
            if not file_path.endswith('.gpl'):
                file_path += '.gpl'

            try:
                with open(file_path, 'w') as f:
                    f.write("GIMP Palette\n")
                    f.write(f"Name: {self.current_palette.name}\n")
                    f.write("#\n")
                    for color in self.current_palette.colors:
                        f.write(f"{color.red():3d} {color.green():3d} {color.blue():3d}\n")

                QMessageBox.information(self, "Success", "Palette saved successfully!")
            except Exception as e:
                QMessageBox.critical(self, "Error", f"Failed to save palette: {str(e)}")

    def _set_tool(self, tool: str):
        """Set current tool"""
        if self.canvas_widget:
            self.canvas_widget.current_tool = tool

    def _on_pen_size_changed(self, value: int):
        """Handle pen size change"""
        if self.canvas_widget:
            self.canvas_widget.pen_size = value

    def _add_layer(self):
        """Add a new layer"""
        if self.canvas_widget:
            layer_num = len(self.canvas_widget.layers) + 1
            layer = Layer(f"Layer {layer_num}",
                         self.canvas_widget.canvas_width,
                         self.canvas_widget.canvas_height)
            cmd = AddLayerCommand(self.canvas_widget, layer, 0)
            self.canvas_widget.command_history.execute(cmd)
            self._update_layer_list()
            self.canvas_widget.update()

    def _remove_layer(self):
        """Remove selected layer"""
        if self.canvas_widget and len(self.canvas_widget.layers) > 1:
            index = self.canvas_widget.current_layer_index
            layer = self.canvas_widget.layers[index]
            cmd = RemoveLayerCommand(self.canvas_widget, layer, index)
            self.canvas_widget.command_history.execute(cmd)
            self._update_layer_list()
            self.canvas_widget.update()

    def _duplicate_layer(self):
        """Duplicate selected layer"""
        if self.canvas_widget:
            current_layer = self.canvas_widget.get_current_layer()
            if current_layer:
                new_layer = current_layer.copy()
                cmd = AddLayerCommand(self.canvas_widget, new_layer,
                                     self.canvas_widget.current_layer_index + 1)
                self.canvas_widget.command_history.execute(cmd)
                self._update_layer_list()
                self.canvas_widget.update()

    def _on_layer_selected(self, index: int):
        """Handle layer selection"""
        if self.canvas_widget and index >= 0:
            self.canvas_widget.current_layer_index = index
            self._update_layer_properties_ui()
            self.canvas_widget.update()

    def _update_layer_properties_ui(self):
        """Update layer properties UI from current layer"""
        if not self.canvas_widget:
            return

        layer = self.canvas_widget.get_current_layer()
        if not layer:
            return

        # Block signals to prevent triggering changes
        self.layer_name_edit.blockSignals(True)
        self.layer_opacity_slider.blockSignals(True)
        self.layer_blend_combo.blockSignals(True)
        self.layer_visible_check.blockSignals(True)
        self.layer_locked_check.blockSignals(True)
        self.layer_alpha_lock_check.blockSignals(True)
        self.layer_clipping_check.blockSignals(True)

        # Update UI
        self.layer_name_edit.setText(layer.name)
        self.layer_opacity_slider.setValue(int(layer.opacity * 100))
        self.layer_opacity_label.setText(f"{int(layer.opacity * 100)}%")
        self.layer_blend_combo.setCurrentText(layer.blend_mode)
        self.layer_visible_check.setChecked(layer.visible)
        self.layer_locked_check.setChecked(layer.locked)
        self.layer_alpha_lock_check.setChecked(layer.alpha_lock)
        self.layer_clipping_check.setChecked(layer.clipping_mask)

        # Unblock signals
        self.layer_name_edit.blockSignals(False)
        self.layer_opacity_slider.blockSignals(False)
        self.layer_blend_combo.blockSignals(False)
        self.layer_visible_check.blockSignals(False)
        self.layer_locked_check.blockSignals(False)
        self.layer_alpha_lock_check.blockSignals(False)
        self.layer_clipping_check.blockSignals(False)

    def _on_layer_name_changed(self):
        """Handle layer name change"""
        if not self.canvas_widget:
            return

        layer = self.canvas_widget.get_current_layer()
        if layer:
            new_name = self.layer_name_edit.text()
            if new_name != layer.name:
                cmd = ModifyLayerCommand(layer)
                layer.name = new_name
                self.canvas_widget.command_history.execute(cmd)
                self._update_layer_list()

    def _on_layer_opacity_changed(self, value: int):
        """Handle layer opacity change"""
        if not self.canvas_widget:
            return

        layer = self.canvas_widget.get_current_layer()
        if layer:
            new_opacity = value / 100.0
            if abs(new_opacity - layer.opacity) > 0.01:
                layer.opacity = new_opacity
                self.layer_opacity_label.setText(f"{value}%")
                self.canvas_widget.update()

    def _on_layer_blend_changed(self, blend_mode: str):
        """Handle layer blend mode change"""
        if not self.canvas_widget:
            return

        layer = self.canvas_widget.get_current_layer()
        if layer and blend_mode != layer.blend_mode:
            cmd = ModifyLayerCommand(layer)
            layer.blend_mode = blend_mode
            self.canvas_widget.command_history.execute(cmd)
            self.canvas_widget.update()

    def _on_layer_visible_changed(self, state: int):
        """Handle layer visibility change"""
        if not self.canvas_widget:
            return

        layer = self.canvas_widget.get_current_layer()
        if layer:
            visible = state == Qt.CheckState.Checked.value
            if visible != layer.visible:
                cmd = ModifyLayerCommand(layer)
                layer.visible = visible
                self.canvas_widget.command_history.execute(cmd)
                self._update_layer_list()
                self.canvas_widget.update()

    def _on_layer_locked_changed(self, state: int):
        """Handle layer locked change"""
        if not self.canvas_widget:
            return

        layer = self.canvas_widget.get_current_layer()
        if layer:
            locked = state == Qt.CheckState.Checked.value
            if locked != layer.locked:
                cmd = ModifyLayerCommand(layer)
                layer.locked = locked
                self.canvas_widget.command_history.execute(cmd)
                self._update_layer_list()

    def _on_layer_alpha_lock_changed(self, state: int):
        """Handle layer alpha lock change"""
        if not self.canvas_widget:
            return

        layer = self.canvas_widget.get_current_layer()
        if layer:
            alpha_lock = state == Qt.CheckState.Checked.value
            if alpha_lock != layer.alpha_lock:
                cmd = ModifyLayerCommand(layer)
                layer.alpha_lock = alpha_lock
                self.canvas_widget.command_history.execute(cmd)
                self._update_layer_list()

    def _on_layer_clipping_changed(self, state: int):
        """Handle layer clipping mask change"""
        if not self.canvas_widget:
            return

        layer = self.canvas_widget.get_current_layer()
        if layer:
            clipping = state == Qt.CheckState.Checked.value
            if clipping != layer.clipping_mask:
                cmd = ModifyLayerCommand(layer)
                layer.clipping_mask = clipping
                self.canvas_widget.command_history.execute(cmd)
                self.canvas_widget.update()

    def _update_layer_list(self):
        """Update layer list widget"""
        if not self.canvas_widget:
            return

        self.layer_list.clear()

        # Add layers in reverse order (top layer first)
        for layer in self.canvas_widget.layers:
            item = QListWidgetItem()

            # Create layer display text
            text = layer.name
            if not layer.visible:
                text += " (hidden)"
            if layer.locked:
                text += " (locked)"
            if layer.alpha_lock:
                text += " [α]"
            if layer.clipping_mask:
                text += " [clip]"

            item.setText(text)
            self.layer_list.addItem(item)

        # Select current layer
        if 0 <= self.canvas_widget.current_layer_index < len(self.canvas_widget.layers):
            self.layer_list.setCurrentRow(self.canvas_widget.current_layer_index)

        # Update properties UI
        self._update_layer_properties_ui()

    def _on_color_changed(self, color: QColor):
        """Handle color change from color wheel"""
        self.canvas_widget.pen_color = color
        self._update_color_display()

    def _on_palette_color_selected(self, color: QColor):
        """Handle color selection from palette"""
        self.canvas_widget.pen_color = color
        self.color_wheel.set_color(color)
        self._update_color_display()

    def _on_color_picked(self, color: QColor):
        """Handle color picked from eyedropper"""
        self.canvas_widget.pen_color = color
        self.color_wheel.set_color(color)
        self._update_color_display()

    def _update_color_display(self):
        """Update color display"""
        color = self.canvas_widget.pen_color if self.canvas_widget else QColor(0, 0, 0)
        self.color_display.setStyleSheet(
            f"background-color: rgb({color.red()}, {color.green()}, {color.blue()}); "
            f"border: 2px solid #ccc;"
        )

    def _pick_color_dialog(self):
        """Show color picker dialog"""
        color = QColorDialog.getColor(self.canvas_widget.pen_color, self, "Pick Color")
        if color.isValid():
            self.canvas_widget.pen_color = color
            self.color_wheel.set_color(color)
            self._update_color_display()

    def _add_current_color_to_palette(self):
        """Add current color to palette"""
        self.palette_widget.add_color(self.canvas_widget.pen_color)
        self.current_palette.add_color(self.canvas_widget.pen_color)

    def _undo(self):
        """Undo"""
        if self.canvas_widget:
            self.canvas_widget.undo()

    def _redo(self):
        """Redo"""
        if self.canvas_widget:
            self.canvas_widget.redo()

    def _on_history_changed(self):
        """Handle history change"""
        if self.canvas_widget:
            self.undo_action.setEnabled(self.canvas_widget.can_undo())
            self.redo_action.setEnabled(self.canvas_widget.can_redo())

    def _reset_view(self):
        """Reset view"""
        if self.canvas_widget:
            self.canvas_widget.reset_view()

    def _toggle_grid(self):
        """Toggle pixel grid"""
        if self.canvas_widget:
            self.canvas_widget.show_pixel_grid = self.grid_action.isChecked()
            self.canvas_widget.update()

    def _zoom_in(self):
        """Zoom in"""
        if self.canvas_widget:
            self.canvas_widget.zoom_level = min(64.0, self.canvas_widget.zoom_level * 1.2)
            self.canvas_widget.update()

    def _zoom_out(self):
        """Zoom out"""
        if self.canvas_widget:
            self.canvas_widget.zoom_level = max(1.0, self.canvas_widget.zoom_level * 0.8)
            self.canvas_widget.update()

    def _toggle_tile_grid(self, state):
        """Toggle tile grid"""
        if self.canvas_widget:
            self.canvas_widget.show_tile_grid = (state == Qt.CheckState.Checked.value)
            self.canvas_widget.update()

    def _on_tile_grid_size_changed(self):
        """Handle tile grid size change"""
        if self.canvas_widget:
            self.canvas_widget.tile_grid_width = self.tile_grid_width_spin.value()
            self.canvas_widget.tile_grid_height = self.tile_grid_height_spin.value()

            # Update tile preview if open
            if self.tile_preview_dialog and self.tile_preview_dialog.isVisible():
                self.tile_preview_dialog.tile_width_spin.setValue(self.tile_grid_width_spin.value())
                self.tile_preview_dialog.tile_height_spin.setValue(self.tile_grid_height_spin.value())

            self.canvas_widget.update()

    def _open_tile_preview(self):
        """Open or focus tile preview window"""
        if not self.canvas_widget:
            return

        if self.tile_preview_dialog is None or not self.tile_preview_dialog.isVisible():
            self.tile_preview_dialog = TilePreviewDialog(self.canvas_widget, self)

            # Sync grid size to preview
            self.tile_preview_dialog.tile_width_spin.setValue(self.tile_grid_width_spin.value())
            self.tile_preview_dialog.tile_height_spin.setValue(self.tile_grid_height_spin.value())

            self.tile_preview_dialog.show()
        else:
            self.tile_preview_dialog.activateWindow()
            self.tile_preview_dialog.raise_()

    def _on_canvas_modified(self):
        """Handle canvas modification"""
        # Update tile preview if open
        if self.tile_preview_dialog and self.tile_preview_dialog.isVisible():
            self.tile_preview_dialog.refresh_preview()

        # Update timeline
        if self.timeline_widget.isVisible():
            self.timeline_widget.update_display()

    def _toggle_timeline(self):
        """Toggle timeline visibility"""
        visible = self.timeline_action.isChecked()
        self.timeline_widget.setVisible(visible)
        if visible and self.canvas_widget:
            self.timeline_widget.set_canvas(self.canvas_widget)
            self.timeline_widget.update_display()

    def _toggle_onion_skinning(self):
        """Toggle onion skinning"""
        if self.canvas_widget:
            self.canvas_widget.onion_skinning = self.onion_skin_action.isChecked()
            self.canvas_widget.update()

    def _on_timeline_frame_selected(self, frame_index: int):
        """Handle frame selection from timeline"""
        if self.canvas_widget:
            self.canvas_widget.set_frame(frame_index)

    def _new_frame(self):
        """Add a new frame"""
        if self.canvas_widget:
            self.canvas_widget.add_frame_to_all_layers()
            self.timeline_widget.update_display()

    def _duplicate_frame(self):
        """Duplicate current frame"""
        if self.canvas_widget:
            self.canvas_widget.duplicate_frame_all_layers(self.canvas_widget.current_frame)
            self.timeline_widget.update_display()

    def _delete_frame(self):
        """Delete current frame"""
        if self.canvas_widget:
            self.canvas_widget.remove_frame_from_all_layers(self.canvas_widget.current_frame)
            self.timeline_widget.update_display()

    def _move_frame_left(self):
        """Move frame left"""
        if self.canvas_widget and self.canvas_widget.current_frame > 0:
            self.canvas_widget.move_frame(self.canvas_widget.current_frame, self.canvas_widget.current_frame - 1)
            self.timeline_widget.update_display()

    def _move_frame_right(self):
        """Move frame right"""
        if self.canvas_widget and self.canvas_widget.current_frame < self.canvas_widget.get_frame_count() - 1:
            self.canvas_widget.move_frame(self.canvas_widget.current_frame, self.canvas_widget.current_frame + 1)
            self.timeline_widget.update_display()

    def _open_animation_preview(self):
        """Open animation preview window"""
        if not self.canvas_widget:
            return

        # Create as independent window with window flags
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

    def _export_spritesheet(self):
        """Export sprite sheet with advanced options"""
        if not self.canvas_widget:
            return

        # Show export dialog
        dialog = SpriteSheetExportDialog(self.canvas_widget, self)
        if dialog.exec() != QDialog.DialogCode.Accepted:
            return

        options = dialog.get_options()

        # Get file path
        file_path, _ = QFileDialog.getSaveFileName(
            self,
            "Export Sprite Sheet",
            str(self.project_root) if self.project_root else "",
            "PNG Files (*.png)"
        )

        if not file_path:
            return

        if not file_path.endswith('.png'):
            file_path += '.png'

        # Collect frames to export
        frames_to_export = []
        frame_data = []  # For JSON metadata

        if options['layout'] == 'per_tag' and self.canvas_widget.animation_tags:
            # Export one row per animation tag
            for tag in self.canvas_widget.animation_tags:
                tag_frames = []
                for i in range(tag.start_frame, tag.end_frame + 1):
                    frame_composite = self.canvas_widget._composite_frame(i)
                    tag_frames.append(frame_composite)
                    frame_data.append({
                        'frame': i,
                        'tag': tag.name,
                        'duration': self.canvas_widget.layers[0].frames[i].duration if self.canvas_widget.layers else 100
                    })
                frames_to_export.append(tag_frames)
        else:
            # Export all frames in single list
            frame_count = self.canvas_widget.get_frame_count()
            all_frames = []
            prev_frame = None

            for i in range(frame_count):
                frame_composite = self.canvas_widget._composite_frame(i)

                # Skip duplicates if option enabled
                if options['skip_duplicates'] and prev_frame is not None:
                    # Convert to QImage for comparison
                    if self._frames_equal(prev_frame, frame_composite):
                        continue

                all_frames.append(frame_composite)
                frame_data.append({
                    'frame': i,
                    'duration': self.canvas_widget.layers[0].frames[i].duration if self.canvas_widget.layers else 100
                })
                prev_frame = frame_composite

            frames_to_export.append(all_frames)

        # Calculate sheet dimensions based on layout
        frame_width = self.canvas_widget.canvas_width
        frame_height = self.canvas_widget.canvas_height

        if options['layout'] == 'horizontal':
            total_frames = sum(len(row) for row in frames_to_export)
            sheet_width = frame_width * total_frames
            sheet_height = frame_height
            sheet = QPixmap(sheet_width, sheet_height)
            sheet.fill(Qt.GlobalColor.transparent)
            painter = QPainter(sheet)
            x_offset = 0
            for row in frames_to_export:
                for frame in row:
                    painter.drawPixmap(x_offset, 0, frame)
                    x_offset += frame_width
            painter.end()

        elif options['layout'] == 'vertical':
            total_frames = sum(len(row) for row in frames_to_export)
            sheet_width = frame_width
            sheet_height = frame_height * total_frames
            sheet = QPixmap(sheet_width, sheet_height)
            sheet.fill(Qt.GlobalColor.transparent)
            painter = QPainter(sheet)
            y_offset = 0
            for row in frames_to_export:
                for frame in row:
                    painter.drawPixmap(0, y_offset, frame)
                    y_offset += frame_height
            painter.end()

        elif options['layout'] == 'grid':
            total_frames = sum(len(row) for row in frames_to_export)
            columns = options['columns']
            rows = (total_frames + columns - 1) // columns  # Ceiling division
            sheet_width = frame_width * columns
            sheet_height = frame_height * rows
            sheet = QPixmap(sheet_width, sheet_height)
            sheet.fill(Qt.GlobalColor.transparent)
            painter = QPainter(sheet)
            frame_idx = 0
            for row in frames_to_export:
                for frame in row:
                    x = (frame_idx % columns) * frame_width
                    y = (frame_idx // columns) * frame_height
                    painter.drawPixmap(x, y, frame)
                    frame_idx += 1
            painter.end()

        elif options['layout'] == 'per_tag':
            # Each tag gets its own row
            max_frames_per_tag = max(len(row) for row in frames_to_export) if frames_to_export else 1
            sheet_width = frame_width * max_frames_per_tag
            sheet_height = frame_height * len(frames_to_export)
            sheet = QPixmap(sheet_width, sheet_height)
            sheet.fill(Qt.GlobalColor.transparent)
            painter = QPainter(sheet)
            for row_idx, row in enumerate(frames_to_export):
                for col_idx, frame in enumerate(row):
                    painter.drawPixmap(col_idx * frame_width, row_idx * frame_height, frame)
            painter.end()

        # Save sprite sheet
        if sheet.save(file_path):
            # Export JSON metadata if requested
            if options['export_json']:
                json_path = file_path.rsplit('.', 1)[0] + '.json'
                metadata = {
                    'frames': frame_data,
                    'meta': {
                        'image': Path(file_path).name,
                        'size': {
                            'w': sheet.width(),
                            'h': sheet.height()
                        },
                        'frame_size': {
                            'w': frame_width,
                            'h': frame_height
                        },
                        'layout': options['layout'],
                        'skip_duplicates': options['skip_duplicates']
                    }
                }

                if self.canvas_widget.animation_tags:
                    metadata['meta']['tags'] = [tag.to_dict() for tag in self.canvas_widget.animation_tags]

                with open(json_path, 'w') as f:
                    json.dump(metadata, f, indent=2)

                QMessageBox.information(self, "Success",
                    f"Sprite sheet exported to:\n{file_path}\n\nJSON metadata: {json_path}")
            else:
                QMessageBox.information(self, "Success", f"Sprite sheet exported to:\n{file_path}")
        else:
            QMessageBox.critical(self, "Error", "Failed to export sprite sheet")

    def _frames_equal(self, frame1: QPixmap, frame2: QPixmap) -> bool:
        """Check if two frames are identical"""
        if frame1.size() != frame2.size():
            return False

        img1 = frame1.toImage()
        img2 = frame2.toImage()

        if img1.size() != img2.size():
            return False

        # Compare pixels
        for y in range(img1.height()):
            for x in range(img1.width()):
                if img1.pixel(x, y) != img2.pixel(x, y):
                    return False

        return True

    def _export_gif(self):
        """Export GIF animation"""
        if not self.canvas_widget:
            return

        if not HAS_PIL:
            QMessageBox.critical(self, "Error",
                "GIF export requires PIL/Pillow library.\n\n"
                "Please install it:\npip install Pillow")
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

        if not file_path.endswith('.gif'):
            file_path += '.gif'

        try:
            # Collect frames
            frame_count = self.canvas_widget.get_frame_count()
            pil_images = []

            for i in range(frame_count):
                # Composite frame
                frame_pixmap = self.canvas_widget._composite_frame(i)

                # Convert QPixmap to PIL Image
                qimage = frame_pixmap.toImage()
                qimage = qimage.convertToFormat(QImage.Format.Format_RGBA8888)

                width = qimage.width()
                height = qimage.height()
                ptr = qimage.bits()
                ptr.setsize(height * width * 4)

                pil_image = Image.frombytes('RGBA', (width, height), ptr, 'raw', 'RGBA')
                pil_images.append(pil_image)

            # Get durations from first layer
            durations = []
            if self.canvas_widget.layers:
                for frame in self.canvas_widget.layers[0].frames:
                    durations.append(frame.duration)
            else:
                durations = [100] * frame_count

            # Save as GIF
            if pil_images:
                pil_images[0].save(
                    file_path,
                    save_all=True,
                    append_images=pil_images[1:],
                    duration=durations,
                    loop=0,
                    disposal=2
                )

                QMessageBox.information(self, "Success", f"GIF exported to:\n{file_path}")
            else:
                QMessageBox.critical(self, "Error", "No frames to export")

        except Exception as e:
            QMessageBox.critical(self, "Error", f"Failed to export GIF:\n{str(e)}")

    def _export_video(self):
        """Export video (MP4/MOV)"""
        if not self.canvas_widget:
            return

        if not HAS_CV2:
            QMessageBox.critical(self, "Error",
                "Video export requires opencv-python library.\n\n"
                "Please install it:\npip install opencv-python")
            return

        # Get export options
        dialog = QDialog(self)
        dialog.setWindowTitle("Export Video")
        layout = QVBoxLayout()

        # Format selection
        format_group = QGroupBox("Format")
        format_layout = QVBoxLayout()

        format_buttons = QButtonGroup()
        mp4_radio = QRadioButton("MP4 (H.264)")
        mp4_radio.setChecked(True)
        format_buttons.addButton(mp4_radio, 0)
        format_layout.addWidget(mp4_radio)

        mov_radio = QRadioButton("MOV (H.264)")
        format_buttons.addButton(mov_radio, 1)
        format_layout.addWidget(mov_radio)

        avi_radio = QRadioButton("AVI (MJPEG)")
        format_buttons.addButton(avi_radio, 2)
        format_layout.addWidget(avi_radio)

        format_group.setLayout(format_layout)
        layout.addWidget(format_group)

        # FPS
        fps_layout = QHBoxLayout()
        fps_layout.addWidget(QLabel("FPS:"))
        fps_spin = QSpinBox()
        fps_spin.setMinimum(1)
        fps_spin.setMaximum(60)
        fps_spin.setValue(10)
        fps_layout.addWidget(fps_spin)
        fps_layout.addStretch()
        layout.addLayout(fps_layout)

        # Dialog buttons
        buttons = QDialogButtonBox(
            QDialogButtonBox.StandardButton.Ok |
            QDialogButtonBox.StandardButton.Cancel
        )
        buttons.accepted.connect(dialog.accept)
        buttons.rejected.connect(dialog.reject)
        layout.addWidget(buttons)

        dialog.setLayout(layout)

        if dialog.exec() != QDialog.DialogCode.Accepted:
            return

        # Get format
        format_id = format_buttons.checkedId()
        if format_id == 0:
            ext = '.mp4'
            fourcc = 'mp4v'
            filter_str = "MP4 Files (*.mp4)"
        elif format_id == 1:
            ext = '.mov'
            fourcc = 'mp4v'
            filter_str = "MOV Files (*.mov)"
        else:
            ext = '.avi'
            fourcc = 'MJPG'
            filter_str = "AVI Files (*.avi)"

        fps = fps_spin.value()

        # Get file path
        file_path, _ = QFileDialog.getSaveFileName(
            self,
            "Export Video",
            str(self.project_root) if self.project_root else "",
            filter_str
        )

        if not file_path:
            return

        if not file_path.endswith(ext):
            file_path += ext

        try:
            # Prepare video writer
            frame_count = self.canvas_widget.get_frame_count()
            width = self.canvas_widget.canvas_width
            height = self.canvas_widget.canvas_height

            # Create video writer
            fourcc_code = cv2.VideoWriter_fourcc(*fourcc)
            out = cv2.VideoWriter(file_path, fourcc_code, fps, (width, height))

            if not out.isOpened():
                QMessageBox.critical(self, "Error", "Failed to create video file")
                return

            # Write frames
            for i in range(frame_count):
                # Composite frame
                frame_pixmap = self.canvas_widget._composite_frame(i)

                # Convert QPixmap to numpy array via QImage
                qimage = frame_pixmap.toImage()
                qimage = qimage.convertToFormat(QImage.Format.Format_RGBA8888)

                width = qimage.width()
                height = qimage.height()
                ptr = qimage.bits()
                ptr.setsize(height * width * 4)

                # Create numpy array
                arr = np.frombuffer(ptr, np.uint8).reshape((height, width, 4))

                # Convert RGBA to BGR for OpenCV
                bgr = cv2.cvtColor(arr, cv2.COLOR_RGBA2BGR)

                # Calculate how many times to repeat frame based on duration
                if self.canvas_widget.layers:
                    duration_ms = self.canvas_widget.layers[0].frames[i].duration
                    num_repeats = max(1, int((duration_ms / 1000.0) * fps))
                else:
                    num_repeats = 1

                # Write frame multiple times based on duration
                for _ in range(num_repeats):
                    out.write(bgr)

            out.release()

            QMessageBox.information(self, "Success", f"Video exported to:\n{file_path}")

        except Exception as e:
            QMessageBox.critical(self, "Error", f"Failed to export video:\n{str(e)}")


# Make the tool discoverable
def create_tool(parent=None, project_root=None):
    """Factory function to create the tool"""
    return PixelPainterTool(parent, project_root)


if __name__ == "__main__":
    from PyQt6.QtWidgets import QApplication
    import sys

    app = QApplication(sys.argv)
    tool = PixelPainterTool()
    tool.show()
    sys.exit(app.exec())
