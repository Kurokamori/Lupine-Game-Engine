"""
Base Editor Panel Class
All editor panels inherit from this class
"""

import sys
from pathlib import Path
from PyQt6.QtWidgets import (QDockWidget, QWidget, QVBoxLayout, QHBoxLayout,
                             QLabel, QPushButton, QMenu, QMainWindow, QDialog,
                             QGridLayout, QFrame, QSizePolicy)
from PyQt6.QtCore import Qt, pyqtSignal
from PyQt6.QtGui import QPainter, QColor, QPen, QBrush

# Add parent directory to path to import theme
sys.path.insert(0, str(Path(__file__).parent.parent))
from theme import get_theme_manager


class DockLocationButton(QPushButton):
    """A button representing a dock location in the visual dock picker"""

    def __init__(self, location_id: str, tooltip: str, parent=None):
        super().__init__(parent)
        self.location_id = location_id
        self.setToolTip(tooltip)
        self.setFixedSize(28, 28)
        self.setCursor(Qt.CursorShape.PointingHandCursor)

        # Get theme colors
        theme_manager = get_theme_manager()
        theme = theme_manager.get_current_theme() if theme_manager else None

        if theme:
            self._bg_color = theme.colors.surface
            self._hover_color = theme.colors.accent_color
            self._border_color = theme.colors.border
        else:
            self._bg_color = "#2d2d2d"
            self._hover_color = "#4a90d9"
            self._border_color = "#555555"

        self.setStyleSheet(f"""
            QPushButton {{
                background-color: {self._bg_color};
                border: 1px solid {self._border_color};
                border-radius: 3px;
            }}
            QPushButton:hover {{
                background-color: {self._hover_color};
                border-color: {self._hover_color};
            }}
        """)


class DockLocationPicker(QDialog):
    """Visual dialog for picking a dock location"""

    location_selected = pyqtSignal(str)  # Emits location ID like "left_all", "bottom_left", etc.

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Dock Location")
        self.setWindowFlags(Qt.WindowType.Popup | Qt.WindowType.FramelessWindowHint)
        self.setFixedSize(220, 180)
        self._setup_ui()

    def _setup_ui(self):
        """Setup the visual dock picker UI"""
        # Get theme colors
        theme_manager = get_theme_manager()
        theme = theme_manager.get_current_theme() if theme_manager else None

        if theme:
            bg_color = theme.colors.surface
            border_color = theme.colors.border
            center_color = theme.colors.secondary_color
            text_color = theme.colors.text_secondary
        else:
            bg_color = "#2d2d2d"
            border_color = "#555555"
            center_color = "#3a3a3a"
            text_color = "#888888"

        self.setStyleSheet(f"""
            QDialog {{
                background-color: {bg_color};
                border: 1px solid {border_color};
                border-radius: 6px;
            }}
            QLabel {{
                color: {text_color};
                font-size: 9px;
                background: transparent;
            }}
        """)

        layout = QGridLayout()
        layout.setContentsMargins(8, 8, 8, 8)
        layout.setSpacing(4)

        # Create the visual grid layout (6 columns x 6 rows):
        # Col:  0      1      2        3      4      5
        # Row 0:      [TopL] [TopM]  [TopR]
        # Row 1: [LA] [LT]   [CENTER------] [RT]   [RA]
        # Row 2: [LA]        [CENTER------]        [RA]
        # Row 3: [LA] [LB]   [CENTER------] [RB]   [RA]
        # Row 4:      [BotL] [BotM]  [BotR]

        # Top row (row 0, columns 1-3)
        self._add_button(layout, "top_left", "Top Left", 0, 1)
        self._add_button(layout, "top_middle", "Top Middle", 0, 2, 1, 2)  # span 2 cols
        self._add_button(layout, "top_right", "Top Right", 0, 4)

        # Left column - full height (column 0, rows 1-3)
        self._add_button(layout, "left_all", "Left (Full)", 1, 0, 3, 1)

        # Left sub-positions (column 1, rows 1 and 3)
        self._add_button(layout, "left_top", "Left Top", 1, 1)
        self._add_button(layout, "left_bottom", "Left Bottom", 3, 1)

        # Center viewport representation (columns 2-3, rows 1-3)
        center_frame = QFrame()
        center_frame.setStyleSheet(f"""
            QFrame {{
                background-color: {center_color};
                border: 1px dashed {border_color};
                border-radius: 3px;
            }}
        """)
        center_frame.setMinimumSize(50, 60)
        center_label = QLabel("Viewport", center_frame)
        center_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        center_layout = QVBoxLayout(center_frame)
        center_layout.addWidget(center_label)
        layout.addWidget(center_frame, 1, 2, 3, 2)  # rows 1-3, cols 2-3

        # Right sub-positions (column 4, rows 1 and 3)
        self._add_button(layout, "right_top", "Right Top", 1, 4)
        self._add_button(layout, "right_bottom", "Right Bottom", 3, 4)

        # Right column - full height (column 5, rows 1-3)
        self._add_button(layout, "right_all", "Right (Full)", 1, 5, 3, 1)

        # Bottom row (row 4, columns 1-4)
        self._add_button(layout, "bottom_left", "Bottom Left", 4, 1)
        self._add_button(layout, "bottom_middle", "Bottom Middle", 4, 2, 1, 2)  # span 2 cols
        self._add_button(layout, "bottom_right", "Bottom Right", 4, 4)

        self.setLayout(layout)

    def _add_button(self, layout: QGridLayout, location_id: str, tooltip: str,
                    row: int, col: int, row_span: int = 1, col_span: int = 1):
        """Add a dock location button to the grid"""
        btn = DockLocationButton(location_id, tooltip, self)
        btn.clicked.connect(lambda: self._on_location_clicked(location_id))
        layout.addWidget(btn, row, col, row_span, col_span)

    def _on_location_clicked(self, location_id: str):
        """Handle location button click"""
        self.location_selected.emit(location_id)
        self.accept()


class PanelTitleBar(QWidget):
    """Custom title bar for editor panels with dock/pop-out buttons"""

    dock_requested = pyqtSignal(str)  # Emits location ID like "left_all", "bottom_left", etc.
    popout_requested = pyqtSignal()
    close_requested = pyqtSignal()

    def __init__(self, title: str, parent=None):
        super().__init__(parent)
        self._title = title
        self._is_floating = False
        self._setup_ui()

    def _setup_ui(self):
        """Setup the title bar UI"""
        layout = QHBoxLayout()
        layout.setContentsMargins(6, 2, 4, 2)
        layout.setSpacing(4)

        # Get theme colors
        theme_manager = get_theme_manager()
        theme = theme_manager.get_current_theme() if theme_manager else None

        if theme:
            bg_color = theme.colors.secondary_color
            text_color = theme.colors.text_primary
            btn_hover = theme.colors.surface_hover
            accent = theme.colors.accent_color
        else:
            bg_color = "#3a3a3a"
            text_color = "#e0e0e0"
            btn_hover = "#4a4a4a"
            accent = "#4a90d9"

        self.setStyleSheet(f"background-color: {bg_color};")

        # Title label
        self._title_label = QLabel(self._title)
        self._title_label.setStyleSheet(f"color: {text_color}; font-weight: 500; background: transparent;")
        layout.addWidget(self._title_label)

        # Spacer
        layout.addStretch()

        # Common button style
        btn_style = f"""
            QPushButton {{
                background-color: transparent;
                color: {text_color};
                border: none;
                padding: 2px 6px;
                font-size: 11px;
                min-width: 20px;
            }}
            QPushButton:hover {{
                background-color: {btn_hover};
                border-radius: 2px;
            }}
            QPushButton:pressed {{
                background-color: {accent};
            }}
        """

        # Dock button (visible when floating) - opens visual dock picker
        self._dock_btn = QPushButton("⊞")  # Grid/dock icon
        self._dock_btn.setToolTip("Dock to main window")
        self._dock_btn.setStyleSheet(btn_style)
        self._dock_btn.clicked.connect(self._show_dock_picker)
        self._dock_btn.setVisible(False)  # Hidden when docked
        layout.addWidget(self._dock_btn)

        # Pop-out button (visible when docked)
        self._popout_btn = QPushButton("⧉")  # Pop-out icon
        self._popout_btn.setToolTip("Pop out to floating window")
        self._popout_btn.setStyleSheet(btn_style)
        self._popout_btn.clicked.connect(lambda: self.popout_requested.emit())
        self._popout_btn.setVisible(True)  # Visible when docked
        layout.addWidget(self._popout_btn)

        # Close button
        self._close_btn = QPushButton("✕")
        self._close_btn.setToolTip("Close panel")
        self._close_btn.setStyleSheet(btn_style)
        self._close_btn.clicked.connect(lambda: self.close_requested.emit())
        layout.addWidget(self._close_btn)

        self.setLayout(layout)

    def _show_dock_picker(self):
        """Show the visual dock location picker"""
        picker = DockLocationPicker(self)
        picker.location_selected.connect(lambda loc: self.dock_requested.emit(loc))

        # Position the picker below the button
        global_pos = self._dock_btn.mapToGlobal(self._dock_btn.rect().bottomLeft())
        picker.move(global_pos.x() - picker.width() + self._dock_btn.width(), global_pos.y())
        picker.exec()

    def set_floating(self, floating: bool):
        """Update button visibility based on floating state"""
        self._is_floating = floating
        self._dock_btn.setVisible(floating)
        self._popout_btn.setVisible(not floating)

    def set_title(self, title: str):
        """Update the title text"""
        self._title = title
        self._title_label.setText(title)


class EditorPanel(QDockWidget):
    """Base class for all editor panels"""

    # Signals
    panel_closed = pyqtSignal(str)  # Emits panel ID when closed
    panel_state_changed = pyqtSignal(str, dict)  # Emits panel ID and state data

    def __init__(self, title: str, parent=None):
        super().__init__(title, parent)
        self._panel_title = title
        self.setAllowedAreas(Qt.DockWidgetArea.AllDockWidgetAreas)
        self.setFeatures(
            QDockWidget.DockWidgetFeature.DockWidgetMovable |
            QDockWidget.DockWidgetFeature.DockWidgetFloatable |
            QDockWidget.DockWidgetFeature.DockWidgetClosable
        )

        # Allow the panel to be a top-level window
        self.setFloating(False)  # Start docked

        # Note: Floating dock crash prevention is handled by FloatingDockGuard
        # in main_editor.py, which monitors all panels and prevents them from
        # docking into each other when floating.

        # Setup custom title bar
        self._title_bar = PanelTitleBar(title, self)
        self._title_bar.dock_requested.connect(self._on_dock_requested)
        self._title_bar.popout_requested.connect(self._on_popout_requested)
        self._title_bar.close_requested.connect(self.close)
        self.setTitleBarWidget(self._title_bar)

        # Connect to floating state changes to update title bar
        self.topLevelChanged.connect(self._on_floating_changed)

        # Create content widget
        self.content_widget = QWidget()
        self.content_layout = QVBoxLayout()
        self.content_layout.setContentsMargins(0, 0, 0, 0)
        self.content_widget.setLayout(self.content_layout)
        self.setWidget(self.content_widget)

        # Setup panel-specific content
        self._setup_panel()

    def handle_save(self) -> bool:
        """Handle a context save (Ctrl+S) request while this panel has focus.

        Panels that own an editable document override this to save it and
        return True. Returning False lets the editor fall back to saving the
        active scene. Centralizing the Ctrl+S binding on the main window this
        way avoids ambiguous shortcut overloads between docked panels.
        """
        return False

    def handle_save_as(self) -> bool:
        """Handle a context save-as (Ctrl+Shift+S) request while this panel has focus.

        Override in panels that support saving to a new path and return True
        when handled; otherwise the editor falls back to scene save-as.
        """
        return False

    def _on_floating_changed(self, floating: bool):
        """Handle floating state changes to update title bar"""
        self._title_bar.set_floating(floating)

    def _on_dock_requested(self, location_id: str):
        """Handle dock request from title bar with location ID"""
        # Find the main window to dock to
        main_window = self._find_main_window()
        if not main_window:
            return

        # Map location IDs to dock areas and positions
        # Format: area_position where position can be "all", "top", "bottom", "left", "middle", "right"
        location_map = {
            # Left positions
            "left_all": (Qt.DockWidgetArea.LeftDockWidgetArea, "all"),
            "left_top": (Qt.DockWidgetArea.LeftDockWidgetArea, "top"),
            "left_bottom": (Qt.DockWidgetArea.LeftDockWidgetArea, "bottom"),
            # Right positions
            "right_all": (Qt.DockWidgetArea.RightDockWidgetArea, "all"),
            "right_top": (Qt.DockWidgetArea.RightDockWidgetArea, "top"),
            "right_bottom": (Qt.DockWidgetArea.RightDockWidgetArea, "bottom"),
            # Top positions
            "top_left": (Qt.DockWidgetArea.TopDockWidgetArea, "left"),
            "top_middle": (Qt.DockWidgetArea.TopDockWidgetArea, "all"),
            "top_right": (Qt.DockWidgetArea.TopDockWidgetArea, "right"),
            # Bottom positions
            "bottom_left": (Qt.DockWidgetArea.BottomDockWidgetArea, "left"),
            "bottom_middle": (Qt.DockWidgetArea.BottomDockWidgetArea, "all"),
            "bottom_right": (Qt.DockWidgetArea.BottomDockWidgetArea, "right"),
        }

        if location_id not in location_map:
            return

        area, position = location_map[location_id]

        # First make sure we're floating
        self.setFloating(True)

        # Position the panel in the dock area
        self._position_in_area(main_window, area, position)

        self.setFloating(False)
        self.show()

    def _position_in_area(self, main_window: QMainWindow, area, position: str):
        """Position this panel within a dock area using splitting"""
        # Find existing docks in this area (excluding self)
        existing_docks = []
        for dock in main_window.findChildren(QDockWidget):
            if dock is not self and dock.isVisible() and not dock.isFloating():
                dock_area = main_window.dockWidgetArea(dock)
                if dock_area == area:
                    existing_docks.append(dock)

        # For "all" position or no existing docks, just add to the area and tabify
        if position == "all" or not existing_docks:
            main_window.addDockWidget(area, self)
            # Tabify with the first existing dock to stack as tabs
            if existing_docks and position == "all":
                main_window.tabifyDockWidget(existing_docks[0], self)
            return

        # Use the first existing dock as the reference for splitting
        reference_dock = existing_docks[0]

        # First add the dock to the area
        main_window.addDockWidget(area, self)

        # Determine split orientation based on area and position
        if area in (Qt.DockWidgetArea.LeftDockWidgetArea, Qt.DockWidgetArea.RightDockWidgetArea):
            # Vertical area - split vertically to create top/bottom sections
            if position == "top":
                # Split: put self above the reference dock
                # splitDockWidget(first, second, orientation) - second goes after first
                # So we need to split with self first, then reference after
                main_window.splitDockWidget(self, reference_dock, Qt.Orientation.Vertical)
            elif position == "bottom":
                # Split: put self below the reference dock
                main_window.splitDockWidget(reference_dock, self, Qt.Orientation.Vertical)
        else:
            # Horizontal area (Top/Bottom) - split horizontally to create left/right sections
            if position == "left":
                # Split: put self to the left of reference
                main_window.splitDockWidget(self, reference_dock, Qt.Orientation.Horizontal)
            elif position == "right":
                # Split: put self to the right of reference
                main_window.splitDockWidget(reference_dock, self, Qt.Orientation.Horizontal)

    def _on_popout_requested(self):
        """Handle pop-out request from title bar"""
        self.setFloating(True)

    def _find_main_window(self):
        """Find the QMainWindow parent"""
        parent = self.parent()
        while parent:
            if isinstance(parent, QMainWindow):
                return parent
            parent = parent.parent()
        return None

    def _setup_panel(self):
        """Override this to setup panel content"""
        pass
    
    def get_panel_id(self) -> str:
        """Get unique identifier for this panel"""
        return self.__class__.__name__
    
    def get_state(self) -> dict:
        """
        Get the current state of this panel for serialization
        Override this to save panel-specific state
        """
        state = {
            "id": self.get_panel_id(),
            "title": self.windowTitle(),
            "visible": self.isVisible(),
            "floating": self.isFloating(),
        }
        # Save geometry for floating panels
        if self.isFloating():
            geo = self.geometry()
            state["geometry"] = {
                "x": geo.x(),
                "y": geo.y(),
                "width": geo.width(),
                "height": geo.height()
            }
        return state

    def set_state(self, state: dict):
        """
        Restore panel state from serialized data
        Override this to restore panel-specific state
        """
        if "floating" in state:
            self.setFloating(state["floating"])
        if "visible" in state:
            self.setVisible(state["visible"])
        # Restore geometry for floating panels
        if state.get("floating") and "geometry" in state:
            geo = state["geometry"]
            self.setGeometry(geo["x"], geo["y"], geo["width"], geo["height"])
    
    def closeEvent(self, event):
        """Handle panel close event"""
        self.panel_closed.emit(self.get_panel_id())
        super().closeEvent(event)
