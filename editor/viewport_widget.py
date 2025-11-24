"""
Viewport Widget
Native window widget for embedding custom rendering
Provides window handle for OpenGL/Vulkan rendering
"""

from PyQt6.QtWidgets import QWidget, QVBoxLayout, QHBoxLayout, QPushButton, QLabel, QTabWidget, QMenu, QMessageBox, QScrollArea, QFrame
from PyQt6.QtCore import Qt, pyqtSignal, QTimer, QPoint, QEvent
from PyQt6.QtGui import QPalette, QColor, QMouseEvent, QWheelEvent, QAction


class ViewportWidget(QWidget):
    """
    Native window widget for rendering
    Does NOT use Qt's OpenGL/Vulkan widgets - provides raw window handle
    """

    # Signals
    viewport_resized = pyqtSignal(int, int)  # width, height
    viewport_2d_toggled = pyqtSignal(bool)  # is 2D mode
    node_selected = pyqtSignal(object)  # selected node (or None for clear selection)
    gizmo_drag_ended = pyqtSignal()  # emitted when gizmo drag operation completes
    polygon_vertex_added = pyqtSignal()  # emitted when a polygon vertex is added

    def __init__(self, scene_name: str = "Untitled", parent=None):
        super().__init__(parent)
        self.scene_name = scene_name
        self.is_2d_mode = False

        # Rendering state
        self.editor_bridge = None
        self.view_id = None
        self.scene = None
        self.render_timer = None
        
        # Selected nodes for multi-selection
        self.selected_nodes = []  # List of selected nodes

        # Mouse input state
        self.last_mouse_pos = QPoint()
        self.middle_mouse_down = False
        self.right_mouse_down = False
        self.left_mouse_down = False
        self.gizmo_dragging = False

        # Polygon creation mode
        self.polygon_creation_mode = False
        self.active_collision_component = None

        # Set black background for native rendering
        palette = self.palette()
        palette.setColor(QPalette.ColorRole.Window, QColor(0, 0, 0))
        self.setAutoFillBackground(True)
        self.setPalette(palette)

        # Native rendering area
        self.render_widget = QWidget(self)
        self.render_widget.setAutoFillBackground(True)
        render_palette = self.render_widget.palette()
        render_palette.setColor(QPalette.ColorRole.Window, QColor(0, 0, 0))
        self.render_widget.setPalette(render_palette)

        # Enable mouse tracking for the render widget
        self.render_widget.setMouseTracking(True)

        # Set default cursor to arrow
        self.render_widget.setCursor(Qt.CursorShape.ArrowCursor)

        # Install event filter to capture mouse events on render_widget
        self.render_widget.installEventFilter(self)

        # Toolbar scroll state
        self.toolbar_scroll_area = None
        self.toolbar_buttons_container = None
        self.left_arrow_btn = None
        self.right_arrow_btn = None

        self._setup_ui()
    
    def _setup_ui(self):
        """Setup viewport UI"""
        layout = QVBoxLayout()
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)
        
        # Toolbar
        toolbar = self._create_toolbar()
        layout.addWidget(toolbar)
        
        # Render area
        layout.addWidget(self.render_widget, stretch=1)
        
        self.setLayout(layout)
    
    def _create_toolbar(self) -> QWidget:
        """Create viewport toolbar"""
        from editor.theme import get_theme_manager
        theme = get_theme_manager().get_current_theme()
        colors = theme.colors

        # Main toolbar container
        toolbar = QWidget()
        toolbar.setFixedHeight(40)
        main_layout = QHBoxLayout()
        main_layout.setContentsMargins(4, 4, 4, 4)
        main_layout.setSpacing(4)

        # Left scroll arrow
        self.left_arrow_btn = QPushButton("<")
        self.left_arrow_btn.setFixedSize(24, 28)
        self.left_arrow_btn.setStyleSheet(f"""
            QPushButton {{
                background-color: {colors.secondary_accent_color};
                color: {colors.text_secondary};
                border: 1px solid {colors.border};
                border-radius: 3px;
                font-weight: bold;
                font-size: 12px;
            }}
            QPushButton:hover {{
                background-color: {colors.secondary_accent_hover};
            }}
            QPushButton:pressed {{
                background-color: {colors.secondary_accent_pressed};
            }}
        """)
        self.left_arrow_btn.clicked.connect(self._scroll_toolbar_left)
        self.left_arrow_btn.setVisible(False)
        main_layout.addWidget(self.left_arrow_btn)

        # Scrollable area for buttons
        self.toolbar_scroll_area = QScrollArea()
        self.toolbar_scroll_area.setHorizontalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAlwaysOff)
        self.toolbar_scroll_area.setVerticalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAlwaysOff)
        self.toolbar_scroll_area.setWidgetResizable(False)  # Don't resize widget - allow overflow
        self.toolbar_scroll_area.setFrameShape(QFrame.Shape.NoFrame)
        self.toolbar_scroll_area.setFixedHeight(36)
        self.toolbar_scroll_area.setStyleSheet("QScrollArea { background: transparent; }")
        self.toolbar_scroll_area.setAlignment(Qt.AlignmentFlag.AlignCenter)

        # Container for buttons
        buttons_container = QWidget()
        toolbar_layout = QHBoxLayout()
        toolbar_layout.setContentsMargins(0, 0, 0, 0)
        toolbar_layout.setSpacing(8)

        # === Group 1: 2D/3D Mode Toggle ===
        # 2D/3D toggle
        self.mode_2d_btn = QPushButton("2D")
        self.mode_2d_btn.setCheckable(True)
        self.mode_2d_btn.setFixedSize(45, 28)
        self.mode_2d_btn.clicked.connect(self._toggle_2d_mode)
        self._apply_toggle_style(self.mode_2d_btn, False, colors)
        toolbar_layout.addWidget(self.mode_2d_btn)

        self.mode_3d_btn = QPushButton("3D")
        self.mode_3d_btn.setCheckable(True)
        self.mode_3d_btn.setChecked(True)
        self.mode_3d_btn.setFixedSize(45, 28)
        self.mode_3d_btn.clicked.connect(self._toggle_3d_mode)
        self._apply_toggle_style(self.mode_3d_btn, True, colors)
        toolbar_layout.addWidget(self.mode_3d_btn)

        # Separator
        self._add_separator(toolbar_layout, colors)

        # === Group 2: View Options ===
        self.grid_btn = QPushButton("Grid")
        self.grid_btn.setCheckable(True)
        self.grid_btn.setChecked(True)
        self.grid_btn.setFixedSize(50, 28)
        self.grid_btn.clicked.connect(self._toggle_grid)
        self._apply_toggle_style(self.grid_btn, True, colors)
        toolbar_layout.addWidget(self.grid_btn)

        # Collision shapes toggle (both 2D and 3D)
        self.collision_shapes_btn = QPushButton("Collision")
        self.collision_shapes_btn.setCheckable(True)
        self.collision_shapes_btn.setChecked(True)
        self.collision_shapes_btn.setFixedSize(75, 28)
        self.collision_shapes_btn.clicked.connect(self._toggle_collision_shapes)
        self._apply_toggle_style(self.collision_shapes_btn, True, colors)
        toolbar_layout.addWidget(self.collision_shapes_btn)

        # Respect Floor toggle (3D only)
        self.respect_floor_btn = QPushButton("Respect Floor")
        self.respect_floor_btn.setCheckable(True)
        self.respect_floor_btn.setChecked(False)
        self.respect_floor_btn.setFixedSize(100, 28)
        self.respect_floor_btn.clicked.connect(self._toggle_respect_floor)
        self.respect_floor_btn.setVisible(not self.is_2d_mode)  # Only visible in 3D
        self._apply_toggle_style(self.respect_floor_btn, False, colors)
        toolbar_layout.addWidget(self.respect_floor_btn)

        # Camera bounds toggle (2D only)
        self.camera_bounds_btn = QPushButton("Camera Bounds")
        self.camera_bounds_btn.setCheckable(True)
        self.camera_bounds_btn.setChecked(True)
        self.camera_bounds_btn.setFixedSize(110, 28)
        self.camera_bounds_btn.clicked.connect(self._toggle_camera_bounds)
        self.camera_bounds_btn.setVisible(self.is_2d_mode)  # Only visible in 2D
        self._apply_toggle_style(self.camera_bounds_btn, True, colors)
        toolbar_layout.addWidget(self.camera_bounds_btn)

        # Separator
        self._add_separator(toolbar_layout, colors)

        # === Group 3: Gizmo Toggle ===
        self.gizmo_btn = QPushButton("Gizmo")
        self.gizmo_btn.setCheckable(True)
        self.gizmo_btn.setChecked(True)
        self.gizmo_btn.setFixedSize(60, 28)
        self.gizmo_btn.clicked.connect(self._toggle_gizmo)
        self._apply_toggle_style(self.gizmo_btn, True, colors)
        toolbar_layout.addWidget(self.gizmo_btn)

        # Separator
        self._add_separator(toolbar_layout, colors)

        # === Group 4: Gizmo Type Selection ===
        self.gizmo_all_btn = QPushButton("All")
        self.gizmo_all_btn.setCheckable(True)
        self.gizmo_all_btn.setChecked(True)
        self.gizmo_all_btn.setFixedSize(45, 28)
        self.gizmo_all_btn.clicked.connect(lambda: self._set_gizmo_type("all"))
        self._apply_toggle_style(self.gizmo_all_btn, True, colors)
        toolbar_layout.addWidget(self.gizmo_all_btn)

        self.gizmo_scale_btn = QPushButton("Scale")
        self.gizmo_scale_btn.setCheckable(True)
        self.gizmo_scale_btn.setFixedSize(55, 28)
        self.gizmo_scale_btn.clicked.connect(lambda: self._set_gizmo_type("scale"))
        self._apply_toggle_style(self.gizmo_scale_btn, False, colors)
        toolbar_layout.addWidget(self.gizmo_scale_btn)

        self.gizmo_move_btn = QPushButton("Move")
        self.gizmo_move_btn.setCheckable(True)
        self.gizmo_move_btn.setFixedSize(55, 28)
        self.gizmo_move_btn.clicked.connect(lambda: self._set_gizmo_type("move"))
        self._apply_toggle_style(self.gizmo_move_btn, False, colors)
        toolbar_layout.addWidget(self.gizmo_move_btn)

        self.gizmo_rotate_btn = QPushButton("Rotate")
        self.gizmo_rotate_btn.setCheckable(True)
        self.gizmo_rotate_btn.setFixedSize(60, 28)
        self.gizmo_rotate_btn.clicked.connect(lambda: self._set_gizmo_type("rotate"))
        self._apply_toggle_style(self.gizmo_rotate_btn, False, colors)
        toolbar_layout.addWidget(self.gizmo_rotate_btn)

        # Separator
        self._add_separator(toolbar_layout, colors)

        # === Group 5: Transform Space Selection ===
        self.space_local_btn = QPushButton("Local")
        self.space_local_btn.setCheckable(True)
        self.space_local_btn.setFixedSize(55, 28)
        self.space_local_btn.clicked.connect(lambda: self._set_transform_space("local"))
        self._apply_toggle_style(self.space_local_btn, False, colors)
        toolbar_layout.addWidget(self.space_local_btn)

        self.space_parent_btn = QPushButton("Parent")
        self.space_parent_btn.setCheckable(True)
        self.space_parent_btn.setFixedSize(60, 28)
        self.space_parent_btn.clicked.connect(lambda: self._set_transform_space("parent"))
        self._apply_toggle_style(self.space_parent_btn, False, colors)
        toolbar_layout.addWidget(self.space_parent_btn)

        self.space_world_btn = QPushButton("World")
        self.space_world_btn.setCheckable(True)
        self.space_world_btn.setChecked(True)  # Default to World
        self.space_world_btn.setFixedSize(55, 28)
        self.space_world_btn.clicked.connect(lambda: self._set_transform_space("world"))
        self._apply_toggle_style(self.space_world_btn, True, colors)
        toolbar_layout.addWidget(self.space_world_btn)

        buttons_container.setLayout(toolbar_layout)
        buttons_container.setFixedSize(buttons_container.sizeHint())

        # Store reference to buttons container
        self.toolbar_buttons_container = buttons_container

        # Set buttons container as scroll area widget
        self.toolbar_scroll_area.setWidget(buttons_container)

        # Install event filter to detect when scrolling is needed
        self.toolbar_scroll_area.installEventFilter(self)

        # Connect scrollbar to update arrows when scrolling
        self.toolbar_scroll_area.horizontalScrollBar().valueChanged.connect(
            lambda: QTimer.singleShot(10, self._update_scroll_arrows)
        )

        main_layout.addWidget(self.toolbar_scroll_area, stretch=1)

        # Right scroll arrow
        self.right_arrow_btn = QPushButton(">")
        self.right_arrow_btn.setFixedSize(24, 28)
        self.right_arrow_btn.setStyleSheet(f"""
            QPushButton {{
                background-color: {colors.secondary_accent_color};
                color: {colors.text_secondary};
                border: 1px solid {colors.border};
                border-radius: 3px;
                font-weight: bold;
                font-size: 12px;
            }}
            QPushButton:hover {{
                background-color: {colors.secondary_accent_hover};
            }}
            QPushButton:pressed {{
                background-color: {colors.secondary_accent_pressed};
            }}
        """)
        self.right_arrow_btn.clicked.connect(self._scroll_toolbar_right)
        self.right_arrow_btn.setVisible(False)
        main_layout.addWidget(self.right_arrow_btn)

        toolbar.setLayout(main_layout)

        # Use a timer to check scroll state after widget is shown
        QTimer.singleShot(100, self._update_scroll_arrows)

        return toolbar

    def _add_separator(self, layout, colors):
        """Add a visual separator to the toolbar"""
        separator = QWidget()
        separator.setFixedSize(1, 24)
        separator.setStyleSheet(f"background-color: {colors.border};")
        layout.addWidget(separator)

    def _apply_toggle_style(self, button, is_active, colors):
        """Apply toggle button styling based on active state"""
        if is_active:
            # Active/enabled state
            button.setStyleSheet(f"""
                QPushButton {{
                    background-color: {colors.accent_color};
                    color: {colors.text_on_accent};
                    border: 1px solid {colors.border};
                    border-radius: 3px;
                    padding: 4px 8px;
                    font-weight: 500;
                    font-size: 11px;
                }}
                QPushButton:hover {{
                    background-color: {colors.accent_hover};
                    border-color: {colors.border_focus};
                }}
                QPushButton:pressed {{
                    background-color: {colors.accent_pressed};
                }}
            """)
        else:
            # Inactive/disabled state
            button.setStyleSheet(f"""
                QPushButton {{
                    background-color: {colors.secondary_accent_color};
                    color: {colors.text_secondary};
                    border: 1px solid {colors.border};
                    border-radius: 3px;
                    padding: 4px 8px;
                    font-weight: 400;
                    font-size: 11px;
                }}
                QPushButton:hover {{
                    background-color: {colors.secondary_accent_hover};
                    border-color: {colors.border_focus};
                }}
                QPushButton:pressed {{
                    background-color: {colors.secondary_accent_pressed};
                }}
            """)

    def _update_button_style(self, button, is_active):
        """Update a button's style based on its active state"""
        from editor.theme import get_theme_manager
        theme = get_theme_manager().get_current_theme()
        colors = theme.colors
        self._apply_toggle_style(button, is_active, colors)
    
    def _toggle_2d_mode(self):
        """Switch to 2D mode"""
        if not self.mode_2d_btn.isChecked():
            self.mode_2d_btn.setChecked(True)
            return

        self.mode_3d_btn.setChecked(False)
        self.is_2d_mode = True
        self.viewport_2d_toggled.emit(True)

        # Update button styles
        self._update_button_style(self.mode_2d_btn, True)
        self._update_button_style(self.mode_3d_btn, False)

        # Show Camera Bounds button in 2D mode
        self.camera_bounds_btn.setVisible(True)
        # Show Collision Shapes button in 2D mode
        self.collision_shapes_btn.setVisible(True)
        # Hide Respect Floor button in 2D mode
        self.respect_floor_btn.setVisible(False)

        # Update button container size and scroll arrows since button visibility changed
        if self.toolbar_buttons_container:
            self.toolbar_buttons_container.setFixedSize(self.toolbar_buttons_container.sizeHint())
        QTimer.singleShot(50, self._update_scroll_arrows)

        # Clear selection when toggling view mode to reset bounding boxes
        self.selected_nodes = []
        if self.editor_bridge and self.view_id:
            self.editor_bridge.set_selected_nodes(self.view_id, [])

        # Update view mode in editor bridge
        if self.editor_bridge and self.view_id:
            import lupine_engine as le
            self.editor_bridge.set_view_mode(self.view_id, le.ViewMode.View2D)

    def _toggle_3d_mode(self):
        """Switch to 3D mode"""
        if not self.mode_3d_btn.isChecked():
            self.mode_3d_btn.setChecked(True)
            return

        self.mode_2d_btn.setChecked(False)
        self.is_2d_mode = False
        self.viewport_2d_toggled.emit(False)

        # Update button styles
        self._update_button_style(self.mode_2d_btn, False)
        self._update_button_style(self.mode_3d_btn, True)

        # Hide Camera Bounds button in 3D mode
        self.camera_bounds_btn.setVisible(False)
        # Show Collision Shapes button in 3D mode
        self.collision_shapes_btn.setVisible(True)
        # Show Respect Floor button in 3D mode
        self.respect_floor_btn.setVisible(True)

        # Update button container size and scroll arrows since button visibility changed
        if self.toolbar_buttons_container:
            self.toolbar_buttons_container.setFixedSize(self.toolbar_buttons_container.sizeHint())
        QTimer.singleShot(50, self._update_scroll_arrows)

        # Clear selection when toggling view mode to reset bounding boxes
        self.selected_nodes = []
        if self.editor_bridge and self.view_id:
            self.editor_bridge.set_selected_nodes(self.view_id, [])

        # Update view mode in editor bridge
        if self.editor_bridge and self.view_id:
            import lupine_engine as le
            self.editor_bridge.set_view_mode(self.view_id, le.ViewMode.View3D)

    def _toggle_grid(self):
        """Toggle grid visibility"""
        enabled = self.grid_btn.isChecked()
        self._update_button_style(self.grid_btn, enabled)
        if self.editor_bridge and self.view_id:
            self.editor_bridge.set_view_grid_rendering_enabled(self.view_id, enabled)

    def _toggle_camera_bounds(self):
        """Toggle camera bounds visibility (2D only)"""
        enabled = self.camera_bounds_btn.isChecked()
        self._update_button_style(self.camera_bounds_btn, enabled)
        if self.editor_bridge and self.view_id:
            self.editor_bridge.set_view_camera_preview_enabled(self.view_id, enabled)

    def _toggle_collision_shapes(self):
        """Toggle collision shapes visibility"""
        enabled = self.collision_shapes_btn.isChecked()
        self._update_button_style(self.collision_shapes_btn, enabled)
        print(f"[ViewportWidget] _toggle_collision_shapes called: enabled={enabled}, editor_bridge={self.editor_bridge is not None}, view_id={self.view_id}")
        if self.editor_bridge and self.view_id:
            self.editor_bridge.set_view_collision_shapes_enabled(self.view_id, enabled)
            print(f"[ViewportWidget] Called set_view_collision_shapes_enabled({self.view_id}, {enabled})")
        else:
            print(f"[ViewportWidget] WARNING: Cannot toggle collision shapes - editor_bridge or view_id not set!")

    def _toggle_gizmo(self):
        """Toggle gizmo visibility"""
        enabled = self.gizmo_btn.isChecked()
        self._update_button_style(self.gizmo_btn, enabled)
        if self.editor_bridge and self.view_id:
            self.editor_bridge.set_gizmo_enabled(self.view_id, enabled)

    def _toggle_respect_floor(self):
        """Toggle respect floor constraint"""
        enabled = self.respect_floor_btn.isChecked()
        self._update_button_style(self.respect_floor_btn, enabled)
        if self.editor_bridge and self.view_id:
            self.editor_bridge.set_respect_floor(self.view_id, enabled)

    def _set_gizmo_type(self, gizmo_type):
        """Set the gizmo type (all, move, rotate, scale)"""
        if self.editor_bridge and self.view_id:
            import lupine_engine as le

            # Uncheck all gizmo type buttons and update styles
            self.gizmo_all_btn.setChecked(False)
            self.gizmo_move_btn.setChecked(False)
            self.gizmo_rotate_btn.setChecked(False)
            self.gizmo_scale_btn.setChecked(False)

            self._update_button_style(self.gizmo_all_btn, False)
            self._update_button_style(self.gizmo_move_btn, False)
            self._update_button_style(self.gizmo_rotate_btn, False)
            self._update_button_style(self.gizmo_scale_btn, False)

            # Set the selected gizmo type
            if gizmo_type == "all":
                self.gizmo_all_btn.setChecked(True)
                self._update_button_style(self.gizmo_all_btn, True)
                self.editor_bridge.set_gizmo_type(self.view_id, le.GizmoType.All)
            elif gizmo_type == "move":
                self.gizmo_move_btn.setChecked(True)
                self._update_button_style(self.gizmo_move_btn, True)
                self.editor_bridge.set_gizmo_type(self.view_id, le.GizmoType.Translation)
            elif gizmo_type == "rotate":
                self.gizmo_rotate_btn.setChecked(True)
                self._update_button_style(self.gizmo_rotate_btn, True)
                self.editor_bridge.set_gizmo_type(self.view_id, le.GizmoType.Rotation)
            elif gizmo_type == "scale":
                self.gizmo_scale_btn.setChecked(True)
                self._update_button_style(self.gizmo_scale_btn, True)
                self.editor_bridge.set_gizmo_type(self.view_id, le.GizmoType.Scale)

    def _set_transform_space(self, space):
        """Set the transform space (local, parent, world)"""
        print(f"[ViewportWidget] _set_transform_space called: space={space}, editor_bridge={self.editor_bridge is not None}, view_id={self.view_id}")
        if self.editor_bridge and self.view_id:
            import lupine_engine as le

            # Uncheck all transform space buttons and update styles
            self.space_local_btn.setChecked(False)
            self.space_parent_btn.setChecked(False)
            self.space_world_btn.setChecked(False)

            self._update_button_style(self.space_local_btn, False)
            self._update_button_style(self.space_parent_btn, False)
            self._update_button_style(self.space_world_btn, False)

            # Set the selected transform space
            if space == "local":
                self.space_local_btn.setChecked(True)
                self._update_button_style(self.space_local_btn, True)
                self.editor_bridge.set_transform_space(self.view_id, le.TransformSpace.Local)
                print(f"[ViewportWidget] Set transform space to Local")
            elif space == "parent":
                self.space_parent_btn.setChecked(True)
                self._update_button_style(self.space_parent_btn, True)
                self.editor_bridge.set_transform_space(self.view_id, le.TransformSpace.Parent)
                print(f"[ViewportWidget] Set transform space to Parent")
            elif space == "world":
                self.space_world_btn.setChecked(True)
                self._update_button_style(self.space_world_btn, True)
                self.editor_bridge.set_transform_space(self.view_id, le.TransformSpace.World)
                print(f"[ViewportWidget] Set transform space to World")
        else:
            print(f"[ViewportWidget] WARNING: Cannot set transform space - editor_bridge or view_id not set!")

    def _scroll_toolbar_left(self):
        """Scroll toolbar content to the left"""
        if self.toolbar_scroll_area:
            scrollbar = self.toolbar_scroll_area.horizontalScrollBar()
            scrollbar.setValue(scrollbar.value() - 100)
            QTimer.singleShot(50, self._update_scroll_arrows)

    def _scroll_toolbar_right(self):
        """Scroll toolbar content to the right"""
        if self.toolbar_scroll_area:
            scrollbar = self.toolbar_scroll_area.horizontalScrollBar()
            scrollbar.setValue(scrollbar.value() + 100)
            QTimer.singleShot(50, self._update_scroll_arrows)

    def _update_scroll_arrows(self):
        """Update visibility of scroll arrows based on content overflow"""
        if not self.toolbar_scroll_area or not self.left_arrow_btn or not self.right_arrow_btn:
            return

        if not self.toolbar_buttons_container:
            return

        scrollbar = self.toolbar_scroll_area.horizontalScrollBar()

        content_width = self.toolbar_buttons_container.width()
        viewport_width = self.toolbar_scroll_area.viewport().width()

        # Show arrows only if content is wider than viewport
        needs_scrolling = content_width > viewport_width

        print(f"[Toolbar] content_width={content_width}, viewport_width={viewport_width}, needs_scrolling={needs_scrolling}")

        if needs_scrolling:
            # Content overflows - show appropriate arrows
            at_left = scrollbar.value() == scrollbar.minimum()
            at_right = scrollbar.value() >= scrollbar.maximum()

            print(f"[Toolbar] scroll_value={scrollbar.value()}, min={scrollbar.minimum()}, max={scrollbar.maximum()}")
            print(f"[Toolbar] at_left={at_left}, at_right={at_right}")
            print(f"[Toolbar] Setting left_arrow visible={not at_left}, right_arrow visible={not at_right}")

            self.left_arrow_btn.setVisible(not at_left)
            self.right_arrow_btn.setVisible(not at_right)
        else:
            # Content fits - hide both arrows (stretches will center it)
            print(f"[Toolbar] Hiding both arrows - content fits")
            self.left_arrow_btn.setVisible(False)
            self.right_arrow_btn.setVisible(False)

    def initialize_rendering(self, editor_bridge, scene=None):
        """Initialize rendering for this viewport"""
        self.editor_bridge = editor_bridge
        self.scene = scene

        # Create render view
        window_handle = self.get_native_handle()
        width, height = self.get_viewport_size()

        self.view_id = self.editor_bridge.create_render_view(window_handle, width, height)
        import lupine_engine as le
        print(f"[ViewportWidget] Created render view: view_id={self.view_id}, scene={self.scene}")

        if self.view_id:
            # Set initial view mode (3D by default)
            import lupine_engine as le
            self.editor_bridge.set_view_mode(self.view_id, le.ViewMode.View3D)

            # Associate scene with view
            if self.scene:
                print(f"[ViewportWidget] Associating scene with view: view_id={self.view_id}, scene={self.scene}")
                self.editor_bridge.set_view_scene(self.view_id, self.scene)
            else:
                le.log_warn(f"[ViewportWidget] No scene to associate with view: view_id={self.view_id}")

            # Enable debug rendering
            self.editor_bridge.set_view_debug_rendering_enabled(self.view_id, True)

            # Start render loop
            self.start_rendering()

    def start_rendering(self):
        """Start the render loop"""
        if not self.render_timer:
            self.render_timer = QTimer(self)
            self.render_timer.timeout.connect(self._render_frame)
            self.render_timer.start(16)  # ~60 FPS

    def stop_rendering(self):
        """Stop the render loop"""
        if self.render_timer:
            self.render_timer.stop()
            self.render_timer = None

    def _render_frame(self):
        """Render a single frame"""
        if self.editor_bridge and self.view_id:
            try:
                self.editor_bridge.render_view(self.view_id)
            except Exception as e:
                print(f"Error rendering frame: {e}")
                import traceback
                traceback.print_exc()
                self.stop_rendering()  # Stop rendering to prevent crash loop

    def set_scene(self, scene):
        """Set the scene to render"""
        self.scene = scene
        import lupine_engine as le
        print(f"[ViewportWidget] set_scene called: view_id={self.view_id}, scene={scene}")
        if self.editor_bridge and self.view_id:
            self.editor_bridge.set_view_scene(self.view_id, scene)

    def cleanup(self):
        """Cleanup rendering resources"""
        self.stop_rendering()

        if self.editor_bridge and self.view_id:
            self.editor_bridge.destroy_render_view(self.view_id)
            self.view_id = None

    def get_native_handle(self) -> int:
        """Get the native window handle for rendering"""
        return int(self.render_widget.winId())
    
    def get_selected_nodes(self):
        """Get list of all selected nodes in this viewport"""
        return self.selected_nodes.copy()

    def get_viewport_size(self) -> tuple:
        """Get viewport dimensions (in physical pixels for high-DPI displays)"""
        # Get device pixel ratio for high-DPI displays
        dpr = self.render_widget.devicePixelRatio()
        logical_width = self.render_widget.width()
        logical_height = self.render_widget.height()

        # Convert to physical pixels
        physical_width = int(logical_width * dpr)
        physical_height = int(logical_height * dpr)

        print(f"Viewport size: logical=({logical_width}x{logical_height}), physical=({physical_width}x{physical_height}), dpr={dpr}")

        return (physical_width, physical_height)

    def resizeEvent(self, event):
        """Handle resize events"""
        super().resizeEvent(event)
        width, height = self.get_viewport_size()
        self.viewport_resized.emit(width, height)

        # Resize render view
        if self.editor_bridge and self.view_id:
            self.editor_bridge.resize_render_view(self.view_id, width, height)

    def eventFilter(self, obj, event):
        """Filter events from render_widget to capture mouse input"""
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
        elif obj == self.toolbar_scroll_area:
            # Update scroll arrows on resize
            if event.type() == QEvent.Type.Resize:
                QTimer.singleShot(50, self._update_scroll_arrows)
        return super().eventFilter(obj, event)

    def _handle_mouse_press(self, event: QMouseEvent):
        """Handle mouse press events"""
        if event.button() == Qt.MouseButton.LeftButton:
            self.left_mouse_down = True
            self.last_mouse_pos = event.pos()

            # Check if we're in polygon creation mode
            if self.polygon_creation_mode and self.is_2d_mode:
                self._add_polygon_vertex(event.pos())
                return

            # First check if we're clicking on a gizmo
            if self.editor_bridge and self.view_id:
                dpr = self.render_widget.devicePixelRatio()
                screen_x = event.pos().x() * dpr
                screen_y = event.pos().y() * dpr

                # Test if gizmo is hit
                if self.editor_bridge.test_gizmo_hit(self.view_id, float(screen_x), float(screen_y)):
                    # Start gizmo drag
                    self.gizmo_dragging = True
                    self.editor_bridge.begin_gizmo_drag(self.view_id, float(screen_x), float(screen_y))
                    return

            # If no gizmo hit, handle node selection
            modifiers = event.modifiers()
            ctrl_held = bool(modifiers & Qt.KeyboardModifier.ControlModifier)
            shift_held = bool(modifiers & Qt.KeyboardModifier.ShiftModifier)
            self._handle_node_picking(event.pos(), ctrl_held, shift_held)

        elif event.button() == Qt.MouseButton.MiddleButton:
            self.middle_mouse_down = True
            self.last_mouse_pos = event.pos()
        elif event.button() == Qt.MouseButton.RightButton:
            self.right_mouse_down = True
            self.last_mouse_pos = event.pos()

    def _handle_mouse_release(self, event: QMouseEvent):
        """Handle mouse release events"""
        if event.button() == Qt.MouseButton.LeftButton:
            self.left_mouse_down = False

            # End gizmo drag if active
            if self.gizmo_dragging and self.editor_bridge and self.view_id:
                self.editor_bridge.end_gizmo_drag(self.view_id)
                self.gizmo_dragging = False
                # Reset cursor to arrow after gizmo drag
                self.render_widget.setCursor(Qt.CursorShape.ArrowCursor)
                # Emit signal to notify that gizmo drag ended (for inspector refresh)
                self.gizmo_drag_ended.emit()

        elif event.button() == Qt.MouseButton.MiddleButton:
            self.middle_mouse_down = False
        elif event.button() == Qt.MouseButton.RightButton:
            self.right_mouse_down = False

    def _handle_mouse_move(self, event: QMouseEvent):
        """Handle mouse move events"""
        if not self.editor_bridge or not self.view_id:
            return

        current_pos = event.pos()
        delta_x = current_pos.x() - self.last_mouse_pos.x()
        delta_y = current_pos.y() - self.last_mouse_pos.y()

        # Handle gizmo dragging first (highest priority)
        if self.gizmo_dragging and self.left_mouse_down:
            dpr = self.render_widget.devicePixelRatio()
            screen_x = current_pos.x() * dpr
            screen_y = current_pos.y() * dpr
            self.editor_bridge.update_gizmo_drag(self.view_id, float(screen_x), float(screen_y))
            return

        if self.middle_mouse_down:
            # Pan camera (both 2D and 3D)
            if self.is_2d_mode:
                self.editor_bridge.pan_camera_2d(self.view_id, float(delta_x), float(delta_y))
            else:
                self.editor_bridge.pan_camera_3d(self.view_id, float(delta_x), float(delta_y))
        elif self.right_mouse_down and not self.is_2d_mode:
            # Orbit camera (3D only)
            self.editor_bridge.orbit_camera_3d(self.view_id, float(delta_x), float(delta_y))

        self.last_mouse_pos = current_pos

    def _handle_wheel(self, event: QWheelEvent):
        """Handle mouse wheel events"""
        if not self.editor_bridge or not self.view_id:
            return

        # Get scroll delta (positive = scroll up/zoom in, negative = scroll down/zoom out)
        delta = event.angleDelta().y() / 120.0  # Standard wheel delta is 120 units per notch

        if self.is_2d_mode:
            self.editor_bridge.zoom_camera_2d(self.view_id, delta)
        else:
            self.editor_bridge.zoom_camera_3d(self.view_id, delta)

    def _handle_node_picking(self, mouse_pos: QPoint, ctrl_held=False, shift_held=False):
        """Handle node picking at mouse position with multi-selection support"""
        if not self.editor_bridge or not self.view_id:
            return

        # Convert logical coordinates to physical coordinates (for high-DPI)
        dpr = self.render_widget.devicePixelRatio()
        screen_x = mouse_pos.x() * dpr
        screen_y = mouse_pos.y() * dpr

        try:
            # Pick node at screen position
            picked_node = self.editor_bridge.pick_node_at_screen_position(
                self.view_id, float(screen_x), float(screen_y)
            )

            # Handle multi-selection
            if ctrl_held or shift_held:
                # Ctrl/Shift: Toggle selection of clicked node
                if picked_node:
                    if picked_node in self.selected_nodes:
                        self.selected_nodes.remove(picked_node)
                    else:
                        self.selected_nodes.append(picked_node)
                # Update editor bridge with all selected nodes
                self.editor_bridge.set_selected_nodes(self.view_id, self.selected_nodes)
            else:
                # No modifier: Single selection (clear others)
                if picked_node:
                    self.selected_nodes = [picked_node]
                else:
                    self.selected_nodes = []
                self.editor_bridge.set_selected_nodes(self.view_id, self.selected_nodes)

            # Emit signal with the picked node for inspector/scene tree updates
            self.node_selected.emit(picked_node)

        except Exception as e:
            print(f"Error picking node: {e}")
            import traceback
            traceback.print_exc()

    def set_polygon_creation_mode(self, enabled):
        """Enable or disable polygon creation mode"""
        self.polygon_creation_mode = enabled

        if enabled:
            # Get the selected node's CollisionBody2D component
            if self.selected_nodes:
                node = self.selected_nodes[0]
                # Find CollisionBody2DComponent on the node
                if self.editor_bridge:
                    try:
                        components = self.editor_bridge.get_components(node)
                        for component in components:
                            if component.get_type_name() == "CollisionBody2DComponent":
                                self.active_collision_component = component
                                print(f"[ViewportWidget] Found CollisionBody2D component on node '{node.get_name()}'")
                                break
                        else:
                            print(f"[ViewportWidget] No CollisionBody2D component found on node '{node.get_name()}'")
                            self.active_collision_component = None
                    except Exception as e:
                        print(f"Error getting CollisionBody2D component: {e}")
                        import traceback
                        traceback.print_exc()
        else:
            self.active_collision_component = None

    def _add_polygon_vertex(self, screen_pos):
        """Add a vertex to the polygon being created"""
        if not self.editor_bridge or not self.view_id or not self.active_collision_component:
            return

        try:
            # Convert screen position to world position
            dpr = self.render_widget.devicePixelRatio()
            screen_x = screen_pos.x() * dpr
            screen_y = screen_pos.y() * dpr

            # DEBUG: Print coordinate information
            print(f"[DEBUG] Polygon vertex placement:")
            print(f"  - Logical click position: ({screen_pos.x()}, {screen_pos.y()})")
            print(f"  - Device pixel ratio: {dpr}")
            print(f"  - Physical position: ({screen_x}, {screen_y})")
            print(f"  - Render widget size (logical): {self.render_widget.width()}x{self.render_widget.height()}")
            print(f"  - Render widget size (physical): {self.render_widget.width() * dpr}x{self.render_widget.height() * dpr}")

            # Get world position from screen position (2D only)
            world_pos = self.editor_bridge.screen_to_world_2d(self.view_id, float(screen_x), float(screen_y))

            print(f"  - World position: ({world_pos['x']:.2f}, {world_pos['y']:.2f})")

            # Convert world position to local position (relative to the node)
            # Get the node that owns the collision component
            if self.selected_nodes:
                node = self.selected_nodes[0]
                # Get node's global position using EditorBridge
                node_pos = self.editor_bridge.get_node_global_position(node)
                rotation = self.editor_bridge.get_node_global_rotation(node)

                # Convert world position to local position (subtract node position)
                local_x = world_pos['x'] - node_pos['x']
                local_y = world_pos['y'] - node_pos['y']

                # Also need to account for node rotation if any
                if abs(rotation) > 0.0001:
                    # Rotate the local position back by the node's rotation
                    import math
                    cos_r = math.cos(-rotation)
                    sin_r = math.sin(-rotation)
                    rotated_x = local_x * cos_r - local_y * sin_r
                    rotated_y = local_x * sin_r + local_y * cos_r
                    local_x = rotated_x
                    local_y = rotated_y

                print(f"  - Node position: ({node_pos['x']:.2f}, {node_pos['y']:.2f})")
                print(f"  - Node rotation: {rotation:.4f} rad")
                print(f"  - Local position: ({local_x:.2f}, {local_y:.2f})")

                # Add vertex in local space
                self.editor_bridge.add_collision_vertex(self.active_collision_component, local_x, local_y)
                print(f"Added polygon vertex at local ({local_x:.2f}, {local_y:.2f})")
            else:
                # Fallback: add in world space (shouldn't happen)
                self.editor_bridge.add_collision_vertex(self.active_collision_component, world_pos['x'], world_pos['y'])
                print(f"Added polygon vertex at world ({world_pos['x']:.2f}, {world_pos['y']:.2f})")

            # Emit signal to notify widget to refresh
            self.polygon_vertex_added.emit()

        except Exception as e:
            print(f"Error adding polygon vertex: {e}")
            import traceback
            traceback.print_exc()


class ViewportTabWidget(QTabWidget):
    """
    Tab widget for managing multiple scene viewports
    """

    # Signals
    scene_tab_changed = pyqtSignal(int)  # tab index
    scene_closed = pyqtSignal(str)  # scene name
    save_scene_requested = pyqtSignal(int)  # tab index
    save_all_scenes_requested = pyqtSignal()
    close_scene_requested = pyqtSignal(int)  # tab index to close
    gizmo_drag_ended = pyqtSignal()  # emitted when gizmo drag operation completes in any viewport

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setTabsClosable(True)
        self.setMovable(True)
        self.setDocumentMode(True)
        self.setContextMenuPolicy(Qt.ContextMenuPolicy.CustomContextMenu)

        self.tabCloseRequested.connect(self._on_tab_close_requested)
        self.currentChanged.connect(self._on_tab_changed)
        self.customContextMenuRequested.connect(self._show_context_menu)

        self.context_menu_tab_index = -1  # Track which tab was right-clicked
    
    def add_scene_viewport(self, scene_name: str) -> ViewportWidget:
        """Add a new scene viewport tab"""
        viewport = ViewportWidget(scene_name, self)
        index = self.addTab(viewport, scene_name)
        self.setCurrentIndex(index)

        # Connect viewport signals to propagate them
        viewport.gizmo_drag_ended.connect(self.gizmo_drag_ended.emit)

        return viewport
    
    def get_current_viewport(self) -> ViewportWidget:
        """Get the currently active viewport"""
        return self.currentWidget()
    
    def get_viewport_by_scene(self, scene_name: str) -> ViewportWidget:
        """Get viewport by scene name"""
        for i in range(self.count()):
            widget = self.widget(i)
            if isinstance(widget, ViewportWidget) and widget.scene_name == scene_name:
                return widget
        return None
    
    def _show_context_menu(self, position):
        """Show context menu for scene tabs"""
        # Find which tab was clicked
        self.context_menu_tab_index = self.tabBar().tabAt(position)

        if self.context_menu_tab_index < 0:
            return

        menu = QMenu(self)

        # Save
        save_action = QAction("Save", self)
        save_action.triggered.connect(lambda: self.save_scene_requested.emit(self.context_menu_tab_index))
        menu.addAction(save_action)

        # Save All
        save_all_action = QAction("Save All", self)
        save_all_action.triggered.connect(lambda: self.save_all_scenes_requested.emit())
        menu.addAction(save_all_action)

        menu.addSeparator()

        # Close
        close_action = QAction("Close", self)
        close_action.triggered.connect(lambda: self.close_scene_requested.emit(self.context_menu_tab_index))
        menu.addAction(close_action)

        # Close All Others
        close_others_action = QAction("Close All Others", self)
        close_others_action.triggered.connect(lambda: self._close_all_others(self.context_menu_tab_index))
        menu.addAction(close_others_action)

        # Close Left Tabs
        close_left_action = QAction("Close Left Tabs", self)
        close_left_action.triggered.connect(lambda: self._close_left_tabs(self.context_menu_tab_index))
        close_left_action.setEnabled(self.context_menu_tab_index > 0)
        menu.addAction(close_left_action)

        # Close Right Tabs
        close_right_action = QAction("Close Right Tabs", self)
        close_right_action.triggered.connect(lambda: self._close_right_tabs(self.context_menu_tab_index))
        close_right_action.setEnabled(self.context_menu_tab_index < self.count() - 1)
        menu.addAction(close_right_action)

        menu.exec(self.tabBar().mapToGlobal(position))

    def _close_all_others(self, keep_index):
        """Close all tabs except the specified one"""
        # Close tabs after keep_index (in reverse to maintain indices)
        for i in range(self.count() - 1, keep_index, -1):
            self.close_scene_requested.emit(i)

        # Close tabs before keep_index (in reverse)
        for i in range(keep_index - 1, -1, -1):
            self.close_scene_requested.emit(i)

    def _close_left_tabs(self, from_index):
        """Close all tabs to the left of the specified index"""
        for i in range(from_index - 1, -1, -1):
            self.close_scene_requested.emit(i)

    def _close_right_tabs(self, from_index):
        """Close all tabs to the right of the specified index"""
        for i in range(self.count() - 1, from_index, -1):
            self.close_scene_requested.emit(i)

    def _on_tab_close_requested(self, index: int):
        """Handle tab close request"""
        # Emit signal instead of directly closing
        self.close_scene_requested.emit(index)

    def close_tab(self, index: int):
        """Actually close the tab (called after confirmation)"""
        widget = self.widget(index)
        if isinstance(widget, ViewportWidget):
            widget.cleanup()  # Cleanup rendering resources
            self.scene_closed.emit(widget.scene_name)
        self.removeTab(index)

    def _on_tab_changed(self, index: int):
        """Handle tab change"""
        self.scene_tab_changed.emit(index)

    def cleanup_all(self):
        """Cleanup all viewport widgets"""
        for i in range(self.count()):
            widget = self.widget(i)
            if isinstance(widget, ViewportWidget):
                widget.cleanup()
