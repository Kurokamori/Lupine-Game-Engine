"""
Lupine Engine Main Editor
The primary editor interface for managing projects and scenes
"""

from PyQt6.QtWidgets import (QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
                             QMenuBar, QMenu, QToolBar, QPushButton, QStatusBar,
                             QMessageBox, QTabWidget, QFileDialog, QLineEdit, QTextEdit, QPlainTextEdit,
                             QSpinBox, QDoubleSpinBox)
from PyQt6.QtCore import Qt, pyqtSignal, QTimer, QSize, QFileSystemWatcher, QEvent
from PyQt6.QtGui import QAction, QKeySequence, QIcon, QKeyEvent
from pathlib import Path
import json
import lupine_engine as le

from panels import (SceneTreePanel, AssetBrowserPanel, FileBrowserPanel,
                   InspectorPanel, ScriptEditorPanel, ConsolePanel)
from tools import NotepadTool, TodoListTool, SpriteAnimatorTool, AssetManager, ScribblerTool, Tileset2DBuilder, TileMap2DEditor, PixelPainterTool, VoxelBuilderTool, FeatureBugTrackerTool, GlobalsManagerTool
from viewport_widget import ViewportTabWidget
from project_file import ProjectData, ProjectFile
from dialogs import ProjectSettingsDialog, InputMappingDialog, ComingSoonDialog
from runtime_controller import RuntimeController
from undo_manager import UndoManager


class MainEditor(QMainWindow):
    """Main editor window"""
    
    # Signals
    project_closed = pyqtSignal()
    
    def __init__(self, project: ProjectData, parent=None):
        super().__init__(parent)
        self.project = project
        self.panels = {}

        # Initialize runtime controller
        self.runtime = RuntimeController()

        # Initialize editor session and bridge
        self.editor_session = le.EditorSession()
        self.editor_session.initialize()
        self.editor_bridge = self.editor_session.get_editor_bridge()
        self.editor_bridge.initialize(le.GraphicsBackend.OpenGL)

        # Initialize undo/redo manager
        self.undo_manager = UndoManager(self.editor_bridge)

        # Install event filter to intercept Ctrl+Z in text fields
        self.installEventFilter(self)

        # Set max undo steps from project settings
        max_undo_steps = getattr(self.project, 'max_undo_steps', 100)
        self.editor_bridge.set_max_undo_steps(max_undo_steps)

        # Set initial project settings
        self.editor_bridge.set_project_settings(
            self.project.window_width,
            self.project.window_height
        )

        # Set texture filtering
        texture_filtering_map = {
            "nearest": (le.FilterMode.Nearest, le.FilterMode.Nearest),
            "bilinear": (le.FilterMode.Linear, le.FilterMode.Linear),
            "cubic": (le.FilterMode.Linear, le.FilterMode.Linear)  # Cubic not yet implemented, use linear
        }
        min_filter, mag_filter = texture_filtering_map.get(self.project.texture_filtering.lower(), (le.FilterMode.Linear, le.FilterMode.Linear))
        self.editor_bridge.set_texture_filtering(min_filter, mag_filter)

        # Scene management
        self.scene_viewports = {}  # Maps scene path to viewport widget

        # File watchers for auto-reloading types
        self.file_watcher = QFileSystemWatcher()
        self._setup_file_watchers()

        self.setWindowTitle(f"Lupine Engine - {project.name}")
        self.setMinimumSize(1280, 720)

        # Enable animated docks and nested docks for better UX
        self.setDockOptions(
            QMainWindow.DockOption.AnimatedDocks |
            QMainWindow.DockOption.AllowNestedDocks |
            QMainWindow.DockOption.AllowTabbedDocks |
            QMainWindow.DockOption.GroupedDragging
        )

        # Set tab positions for all dock areas (South = bottom)
        self.setTabPosition(Qt.DockWidgetArea.LeftDockWidgetArea, QTabWidget.TabPosition.South)
        self.setTabPosition(Qt.DockWidgetArea.RightDockWidgetArea, QTabWidget.TabPosition.South)
        self.setTabPosition(Qt.DockWidgetArea.TopDockWidgetArea, QTabWidget.TabPosition.South)
        self.setTabPosition(Qt.DockWidgetArea.BottomDockWidgetArea, QTabWidget.TabPosition.South)

        # Set project settings in bridge
        window_width = project.window_width if hasattr(project, 'window_width') else 1920
        window_height = project.window_height if hasattr(project, 'window_height') else 1080
        self.editor_bridge.set_project_settings(window_width, window_height)

        self._setup_ui()
        self._create_panels()
        self._setup_default_layout()
        self._load_main_scene()
    
    def _setup_ui(self):
        """Setup main editor UI"""
        # Create menu bar
        self._create_menu_bar()

        # Create game runner toolbar
        self._create_game_toolbar()

        # Create central viewport area
        self.viewport_tabs = ViewportTabWidget()
        self.viewport_tabs.scene_tab_changed.connect(self._on_scene_tab_changed)
        self.viewport_tabs.scene_closed.connect(self._on_scene_closed)
        self.viewport_tabs.save_scene_requested.connect(self._on_save_scene_requested)
        self.viewport_tabs.save_all_scenes_requested.connect(self._save_all_scenes)
        self.viewport_tabs.close_scene_requested.connect(self._on_close_scene_requested)
        self.viewport_tabs.gizmo_drag_ended.connect(self._on_gizmo_drag_ended)
        self.setCentralWidget(self.viewport_tabs)

        # Create status bar
        self.status_bar = QStatusBar()
        self.setStatusBar(self.status_bar)
        self.status_bar.showMessage("Ready")
    
    def _create_menu_bar(self):
        """Create the main menu bar"""
        menubar = self.menuBar()
        
        # File menu
        file_menu = menubar.addMenu("File")
        
        new_scene_action = QAction("New Scene", self)
        new_scene_action.setShortcut(QKeySequence("Ctrl+N"))
        new_scene_action.triggered.connect(self._new_scene)
        file_menu.addAction(new_scene_action)

        open_scene_action = QAction("Open Scene", self)
        open_scene_action.setShortcut(QKeySequence("Ctrl+O"))
        open_scene_action.triggered.connect(self._open_scene)
        file_menu.addAction(open_scene_action)

        file_menu.addSeparator()

        save_scene_action = QAction("Save Scene", self)
        save_scene_action.setShortcut(QKeySequence("Ctrl+S"))
        save_scene_action.triggered.connect(self._save_scene)
        file_menu.addAction(save_scene_action)

        save_scene_as_action = QAction("Save Scene As...", self)
        save_scene_as_action.setShortcut(QKeySequence("Ctrl+Shift+S"))
        save_scene_as_action.triggered.connect(self._save_scene_as)
        file_menu.addAction(save_scene_as_action)

        save_all_action = QAction("Save All", self)
        save_all_action.setShortcut(QKeySequence("Ctrl+Alt+S"))
        save_all_action.triggered.connect(self._save_all_scenes)
        file_menu.addAction(save_all_action)
        
        file_menu.addSeparator()

        project_settings_action = QAction("Project Settings", self)
        project_settings_action.triggered.connect(self._open_project_settings)
        file_menu.addAction(project_settings_action)

        file_menu.addSeparator()
        
        close_project_action = QAction("Close Project", self)
        close_project_action.triggered.connect(self._close_project)
        file_menu.addAction(close_project_action)
        
        exit_action = QAction("Exit", self)
        exit_action.setShortcut(QKeySequence("Alt+F4"))
        exit_action.triggered.connect(self.close)
        file_menu.addAction(exit_action)
        
        # Edit menu
        edit_menu = menubar.addMenu("Edit")

        self.undo_action = QAction("Undo", self)
        self.undo_action.setShortcut(QKeySequence("Ctrl+Z"))
        self.undo_action.setShortcutContext(Qt.ShortcutContext.ApplicationShortcut)
        self.undo_action.triggered.connect(self._on_undo)
        edit_menu.addAction(self.undo_action)
        self.addAction(self.undo_action)  # Add to main window for global shortcut

        self.redo_action = QAction("Redo", self)
        self.redo_action.setShortcut(QKeySequence("Ctrl+Shift+Z"))
        self.redo_action.setShortcutContext(Qt.ShortcutContext.ApplicationShortcut)
        self.redo_action.triggered.connect(self._on_redo)
        edit_menu.addAction(self.redo_action)
        self.addAction(self.redo_action)  # Add to main window for global shortcut
        
        edit_menu.addSeparator()

        self.cut_action = QAction("Cut", self)
        self.cut_action.setShortcut(QKeySequence("Ctrl+X"))
        self.cut_action.triggered.connect(self._on_cut)
        edit_menu.addAction(self.cut_action)

        self.copy_action = QAction("Copy", self)
        self.copy_action.setShortcut(QKeySequence("Ctrl+C"))
        self.copy_action.triggered.connect(self._on_copy)
        edit_menu.addAction(self.copy_action)

        self.paste_action = QAction("Paste", self)
        self.paste_action.setShortcut(QKeySequence("Ctrl+V"))
        self.paste_action.triggered.connect(self._on_paste)
        edit_menu.addAction(self.paste_action)

        self.delete_action = QAction("Delete", self)
        self.delete_action.setShortcut(QKeySequence("Delete"))
        self.delete_action.triggered.connect(self._on_delete)
        edit_menu.addAction(self.delete_action)

        edit_menu.addSeparator()

        self.duplicate_action = QAction("Duplicate", self)
        self.duplicate_action.setShortcut(QKeySequence("Ctrl+D"))
        self.duplicate_action.triggered.connect(self._on_duplicate)
        edit_menu.addAction(self.duplicate_action)

        edit_menu.addSeparator()

        preferences_action = QAction("Preferences", self)
        edit_menu.addAction(preferences_action)
        
        # View menu
        view_menu = menubar.addMenu("View")
        
        self.view_scene_tree_action = QAction("Scene Tree", self)
        self.view_scene_tree_action.setCheckable(True)
        self.view_scene_tree_action.setChecked(True)
        self.view_scene_tree_action.triggered.connect(lambda: self._toggle_panel('scene_tree'))
        view_menu.addAction(self.view_scene_tree_action)
        
        self.view_asset_browser_action = QAction("Asset Browser", self)
        self.view_asset_browser_action.setCheckable(True)
        self.view_asset_browser_action.setChecked(True)
        self.view_asset_browser_action.triggered.connect(lambda: self._toggle_panel('asset_browser'))
        view_menu.addAction(self.view_asset_browser_action)
        
        self.view_file_browser_action = QAction("File Browser", self)
        self.view_file_browser_action.setCheckable(True)
        self.view_file_browser_action.setChecked(True)
        self.view_file_browser_action.triggered.connect(lambda: self._toggle_panel('file_browser'))
        view_menu.addAction(self.view_file_browser_action)
        
        self.view_inspector_action = QAction("Inspector", self)
        self.view_inspector_action.setCheckable(True)
        self.view_inspector_action.setChecked(True)
        self.view_inspector_action.triggered.connect(lambda: self._toggle_panel('inspector'))
        view_menu.addAction(self.view_inspector_action)
        
        self.view_script_editor_action = QAction("Script Editor", self)
        self.view_script_editor_action.setCheckable(True)
        self.view_script_editor_action.setChecked(True)
        self.view_script_editor_action.triggered.connect(lambda: self._toggle_panel('script_editor'))
        view_menu.addAction(self.view_script_editor_action)
        
        self.view_console_action = QAction("Console", self)
        self.view_console_action.setCheckable(True)
        self.view_console_action.setChecked(True)
        self.view_console_action.triggered.connect(lambda: self._toggle_panel('console'))
        view_menu.addAction(self.view_console_action)

        view_menu.addSeparator()
        
        reset_layout_action = QAction("Reset Layout", self)
        reset_layout_action.triggered.connect(self._setup_default_layout)
        view_menu.addAction(reset_layout_action)
        
        save_layout_action = QAction("Save Layout", self)
        save_layout_action.triggered.connect(self._save_layout)
        view_menu.addAction(save_layout_action)
        
        # Tools menu
        tools_menu = menubar.addMenu("Tools")

        # Animators submenu
        animators_menu = tools_menu.addMenu("Animators")

        tween_animator_action = QAction("Tween Animator", self)
        tween_animator_action.triggered.connect(lambda: self._show_coming_soon("Tween Animator"))
        animators_menu.addAction(tween_animator_action)

        sprite_sheet_animator_action = QAction("Sprite Animator", self)
        sprite_sheet_animator_action.triggered.connect(self._show_sprite_animator)
        animators_menu.addAction(sprite_sheet_animator_action)

        state_animator_action = QAction("State Animator", self)
        state_animator_action.triggered.connect(lambda: self._show_coming_soon("State Animator"))
        animators_menu.addAction(state_animator_action)

        # Tiles submenu
        tiles_menu = tools_menu.addMenu("Tiles")

        tileset_2d_action = QAction("Tileset 2D", self)
        tileset_2d_action.triggered.connect(self._show_tileset_builder)
        tiles_menu.addAction(tileset_2d_action)

        tilemap_2d_action = QAction("Tilemap 2D", self)
        tilemap_2d_action.triggered.connect(self._show_tilemap_editor)
        tiles_menu.addAction(tilemap_2d_action)

        tilemap_25d_action = QAction("Tilemap 2.5D", self)
        tilemap_25d_action.triggered.connect(lambda: self._show_coming_soon("Tilemap 2.5D"))
        tiles_menu.addAction(tilemap_25d_action)

        tileset_3d_action = QAction("Tileset 3D", self)
        tileset_3d_action.triggered.connect(lambda: self._show_coming_soon("Tileset 3D"))
        tiles_menu.addAction(tileset_3d_action)

        tile_builder_3d_action = QAction("3D Tile Builder", self)
        tile_builder_3d_action.triggered.connect(lambda: self._show_coming_soon("3D Tile Builder"))
        tiles_menu.addAction(tile_builder_3d_action)

        tilemap_3d_action = QAction("Tilemap 3D", self)
        tilemap_3d_action.triggered.connect(lambda: self._show_coming_soon("Tilemap 3D"))
        tiles_menu.addAction(tilemap_3d_action)

        # Art Tools submenu
        art_tools_menu = tools_menu.addMenu("Art Tools")

        pixel_painter_action = QAction("Pixel Painter", self)
        pixel_painter_action.triggered.connect(self._show_pixel_painter)
        art_tools_menu.addAction(pixel_painter_action)

        scribbler_action = QAction("Scribbler", self)
        scribbler_action.triggered.connect(self._show_scribbler)
        art_tools_menu.addAction(scribbler_action)

        voxel_builder_action = QAction("Voxel Builder", self)
        voxel_builder_action.triggered.connect(self._show_voxel_builder)
        art_tools_menu.addAction(voxel_builder_action)

        # Visual Scripter (top-level)
        visual_scripter_action = QAction("Visual Scripter", self)
        visual_scripter_action.triggered.connect(lambda: self._show_coming_soon("Visual Scripter"))
        tools_menu.addAction(visual_scripter_action)

        # Globals Manager (top-level)
        globals_manager_action = QAction("Globals Manager", self)
        globals_manager_action.triggered.connect(self._show_globals_manager)
        tools_menu.addAction(globals_manager_action)

        # Input Mapper (top-level) - already implemented
        input_mapper_action = QAction("Input Mapper", self)
        input_mapper_action.setShortcut(QKeySequence("Ctrl+Shift+I"))
        input_mapper_action.triggered.connect(self._open_input_map)
        tools_menu.addAction(input_mapper_action)

        # Scriptable Objects (top-level)
        scriptable_objects_action = QAction("Scriptable Objects", self)
        scriptable_objects_action.triggered.connect(lambda: self._show_coming_soon("Scriptable Objects"))
        tools_menu.addAction(scriptable_objects_action)

        # Builders submenu
        builders_menu = tools_menu.addMenu("Builders")

        menu_builder_action = QAction("Menu Builder", self)
        menu_builder_action.triggered.connect(lambda: self._show_coming_soon("Menu Builder"))
        builders_menu.addAction(menu_builder_action)

        terrain_builder_action = QAction("Terrain Builder", self)
        terrain_builder_action.triggered.connect(lambda: self._show_coming_soon("Terrain Builder"))
        builders_menu.addAction(terrain_builder_action)

        state_machine_designer_action = QAction("State Machine Designer", self)
        state_machine_designer_action.triggered.connect(lambda: self._show_coming_soon("State Machine Designer"))
        builders_menu.addAction(state_machine_designer_action)

        # Localization (top-level)
        localization_action = QAction("Localization", self)
        localization_action.triggered.connect(lambda: self._show_coming_soon("Localization"))
        tools_menu.addAction(localization_action)

        # Audio submenu
        audio_menu = tools_menu.addMenu("Audio")

        mixer_action = QAction("Mixer", self)
        mixer_action.triggered.connect(lambda: self._show_coming_soon("Mixer"))
        audio_menu.addAction(mixer_action)

        simple_daw_action = QAction("SimpleDAW", self)
        simple_daw_action.triggered.connect(lambda: self._show_coming_soon("SimpleDAW"))
        audio_menu.addAction(simple_daw_action)

        # Production submenu
        production_menu = tools_menu.addMenu("Production")

        notepad_action = QAction("Notepad", self)
        notepad_action.triggered.connect(self._show_notepad)
        production_menu.addAction(notepad_action)

        todo_list_action = QAction("To Do Lists", self)
        todo_list_action.triggered.connect(self._show_todo_list)
        production_menu.addAction(todo_list_action)

        feature_bug_tracker_action = QAction("Feature Bug Tracker", self)
        feature_bug_tracker_action.triggered.connect(self._show_feature_bug_tracker)
        production_menu.addAction(feature_bug_tracker_action)

        asset_manager_action = QAction("Asset Manager", self)
        asset_manager_action.triggered.connect(self._show_asset_manager)
        production_menu.addAction(asset_manager_action)

        globals_manager_action = QAction("Globals Manager", self)
        globals_manager_action.triggered.connect(self._show_globals_manager)
        production_menu.addAction(globals_manager_action)


        # Help menu
        help_menu = menubar.addMenu("Help")
        
        documentation_action = QAction("Documentation", self)
        help_menu.addAction(documentation_action)
        
        api_reference_action = QAction("API Reference", self)
        help_menu.addAction(api_reference_action)
        
        help_menu.addSeparator()
        
        about_action = QAction("About Lupine Engine", self)
        about_action.triggered.connect(self._show_about)
        help_menu.addAction(about_action)
    
    def _create_game_toolbar(self):
        """Create game runner toolbar"""
        toolbar = QToolBar("Game Runner")
        toolbar.setObjectName("GameRunnerToolbar")
        toolbar.setMovable(True)  # Allow toolbar to be moved
        toolbar.setFloatable(True)  # Allow toolbar to be detached
        toolbar.setIconSize(QSize(16, 16))
        toolbar.setToolButtonStyle(Qt.ToolButtonStyle.ToolButtonTextBesideIcon)
        self.addToolBar(Qt.ToolBarArea.TopToolBarArea, toolbar)

        # Play game button
        self.play_game_btn = QPushButton("▶ Play Game")
        self.play_game_btn.setToolTip("Run the game from main scene")
        self.play_game_btn.setFixedHeight(28)
        self.play_game_btn.clicked.connect(self._on_play_game)
        toolbar.addWidget(self.play_game_btn)

        # Play scene button
        self.play_scene_btn = QPushButton("▶ Play Scene")
        self.play_scene_btn.setToolTip("Run current scene")
        self.play_scene_btn.setFixedHeight(28)
        self.play_scene_btn.clicked.connect(self._on_play_scene)
        toolbar.addWidget(self.play_scene_btn)

        toolbar.addSeparator()

        # Relaunch button
        self.relaunch_btn = QPushButton("⟳ Relaunch")
        self.relaunch_btn.setToolTip("Restart the game")
        self.relaunch_btn.setEnabled(False)
        self.relaunch_btn.setFixedHeight(28)
        self.relaunch_btn.clicked.connect(self._on_relaunch)
        toolbar.addWidget(self.relaunch_btn)

        # Pause button
        self.pause_btn = QPushButton("⏸ Pause")
        self.pause_btn.setToolTip("Pause game execution")
        self.pause_btn.setEnabled(False)
        self.pause_btn.setFixedHeight(28)
        self.pause_btn.clicked.connect(self._on_pause)
        toolbar.addWidget(self.pause_btn)

        # Stop button
        self.stop_btn = QPushButton("⏹ Stop")
        self.stop_btn.setToolTip("Stop game execution")
        self.stop_btn.setEnabled(False)
        self.stop_btn.setProperty("danger", True)
        self.stop_btn.setFixedHeight(28)
        self.stop_btn.clicked.connect(self._on_stop)
        toolbar.addWidget(self.stop_btn)

        # Setup timer to update button states
        self.runtime_state_timer = QTimer(self)
        self.runtime_state_timer.timeout.connect(self._update_runtime_button_states)
        self.runtime_state_timer.start(100)  # Check every 100ms
    
    def _create_panels(self):
        """Create all editor panels"""
        # Left panels
        self.panels['scene_tree'] = SceneTreePanel(self)
        self.panels['asset_browser'] = AssetBrowserPanel(self)
        self.panels['file_browser'] = FileBrowserPanel(self)
        
        # Right panels
        self.panels['inspector'] = InspectorPanel(self)
        self.panels['script_editor'] = ScriptEditorPanel(self)
        
        # Bottom panel
        self.panels['console'] = ConsolePanel(self)

        # Initialize file browser with project root
        self.panels['file_browser'].set_project_root(self.project.path)

        # Initialize script editor with project root
        self.panels['script_editor'].project_root = self.project.get_directory()

        # Initialize scene tree panel with editor bridge
        self.panels['scene_tree'].editor_bridge = self.editor_bridge

        # Initialize inspector panel with editor bridge
        self.panels['inspector'].editor_bridge = self.editor_bridge

        # Scan project types for node/component/prefab discovery
        self.editor_bridge.scan_project_types(self.project.path)

        # Connect scene tree to inspector
        self.panels['scene_tree'].node_selected.connect(self.panels['inspector'].set_node)
        self.panels['scene_tree'].node_selected.connect(self._on_scene_tree_node_selected)
        self.panels['scene_tree'].node_deleted.connect(self._on_node_deleted)

        # Connect file browser signals
        self.panels['file_browser'].script_opened.connect(self._open_script_in_editor)
        self.panels['file_browser'].scene_opened.connect(self._open_scene_as_tab)
        
        # Connect panel visibility changes to menu actions
        self.panels['scene_tree'].visibilityChanged.connect(
            lambda visible: self.view_scene_tree_action.setChecked(visible))
        self.panels['asset_browser'].visibilityChanged.connect(
            lambda visible: self.view_asset_browser_action.setChecked(visible))
        self.panels['file_browser'].visibilityChanged.connect(
            lambda visible: self.view_file_browser_action.setChecked(visible))
        self.panels['inspector'].visibilityChanged.connect(
            lambda visible: self.view_inspector_action.setChecked(visible))
        self.panels['script_editor'].visibilityChanged.connect(
            lambda visible: self.view_script_editor_action.setChecked(visible))
        self.panels['console'].visibilityChanged.connect(
            lambda visible: self.view_console_action.setChecked(visible))
    
    def _toggle_panel(self, panel_id: str):
        """Toggle panel visibility"""
        if panel_id in self.panels:
            panel = self.panels[panel_id]
            panel.setVisible(not panel.isVisible())
    
    def _setup_default_layout(self):
        """Setup the default panel layout"""
        # Remove all dock widgets first
        for panel in self.panels.values():
            self.removeDockWidget(panel)
        
        # Left side - Scene Tree, Asset Browser, File Browser (stacked)
        self.addDockWidget(Qt.DockWidgetArea.LeftDockWidgetArea, self.panels['scene_tree'])
        self.addDockWidget(Qt.DockWidgetArea.LeftDockWidgetArea, self.panels['asset_browser'])
        self.addDockWidget(Qt.DockWidgetArea.LeftDockWidgetArea, self.panels['file_browser'])
        self.tabifyDockWidget(self.panels['scene_tree'], self.panels['asset_browser'])
        self.tabifyDockWidget(self.panels['asset_browser'], self.panels['file_browser'])
        
        # Right side - Inspector, Script Editor (stacked)
        self.addDockWidget(Qt.DockWidgetArea.RightDockWidgetArea, self.panels['inspector'])
        self.addDockWidget(Qt.DockWidgetArea.RightDockWidgetArea, self.panels['script_editor'])
        self.tabifyDockWidget(self.panels['inspector'], self.panels['script_editor'])
        
        # Bottom - Console
        self.addDockWidget(Qt.DockWidgetArea.BottomDockWidgetArea, self.panels['console'])
        
        # Make sure all panels are visible
        for panel in self.panels.values():
            panel.setVisible(True)
            panel.show()
        
        # Ensure Scene Tree is the active tab on the left
        self.panels['scene_tree'].raise_()
        
        # Ensure Inspector is the active tab on the right
        self.panels['inspector'].raise_()
        
        # Set reasonable sizes for dock areas
        self.resizeDocks([self.panels['scene_tree']], [300], Qt.Orientation.Horizontal)
        self.resizeDocks([self.panels['inspector']], [350], Qt.Orientation.Horizontal)
        self.resizeDocks([self.panels['console']], [200], Qt.Orientation.Vertical)
        
        self.status_bar.showMessage("Layout reset to default", 3000)
    
    def _load_main_scene(self):
        """Load the project's main scene"""
        if self.project.main_scene:
            scene_path = Path(self.project.get_directory()) / self.project.main_scene
            if scene_path.exists():
                scene_name = scene_path.stem
                self._open_scene_viewport(str(scene_path), scene_name)
                self.status_bar.showMessage(f"Loaded scene: {scene_name}", 3000)

                # Log to console
                if 'console' in self.panels:
                    self.panels['console'].log_message(f"Loaded main scene: {scene_name}", "Info")
            else:
                # Create default scene viewport
                self._create_default_scene()
        else:
            # Create default scene viewport
            self._create_default_scene()
    
    def _open_script_in_editor(self, script_path: str):
        """Open a script file in the script editor panel"""
        self.status_bar.showMessage(f"Opening script: {script_path}", 3000)
        
        # Open in script editor
        if 'script_editor' in self.panels:
            self.panels['script_editor'].open_script(script_path)
            self.panels['script_editor'].raise_()
            self.panels['script_editor'].setVisible(True)
        
        if 'console' in self.panels:
            self.panels['console'].log_message(f"Script opened: {script_path}", "Info")
    
    def _open_scene_as_tab(self, scene_path: str):
        """Open a scene file as a new viewport tab"""
        scene_name = Path(scene_path).stem

        # Check if scene is already open
        if scene_path in self.scene_viewports:
            # Scene is already open, switch to that tab
            viewport = self.scene_viewports[scene_path]
            for i in range(self.viewport_tabs.count()):
                if self.viewport_tabs.widget(i) == viewport:
                    self.viewport_tabs.setCurrentIndex(i)
                    self.status_bar.showMessage(f"Switched to scene: {scene_name}", 3000)
                    if 'console' in self.panels:
                        self.panels['console'].log_message(f"Switched to already open scene: {scene_path}", "Info")
                    return

        # Scene is not open, open it
        self._open_scene_viewport(scene_path, scene_name)
        self.status_bar.showMessage(f"Opened scene: {scene_name}", 3000)

        if 'console' in self.panels:
            self.panels['console'].log_message(f"Scene opened: {scene_path}", "Info")

    def _create_default_scene(self):
        """Create a default empty scene"""
        # Create a new scene using editor session
        scene_doc = self.editor_session.create_scene("Untitled")
        if scene_doc:
            scene = scene_doc.get_scene()
            scene_name = scene_doc.get_display_name()

            viewport = self.viewport_tabs.add_scene_viewport(scene_name)
            # Delay rendering initialization to ensure widget is laid out
            from PyQt6.QtCore import QTimer
            QTimer.singleShot(0, lambda: viewport.initialize_rendering(self.editor_bridge, scene))
            self.scene_viewports[scene_name] = viewport

            # Connect viewport node selection to scene tree and inspector
            viewport.node_selected.connect(self._on_viewport_node_selected)

            # Set as active scene
            self.editor_session.set_active_scene(scene_doc.get_id())
            self.editor_bridge.set_active_scene(scene)

            # Update scene tree
            if 'scene_tree' in self.panels:
                self.panels['scene_tree'].set_scene(scene)

    def _open_scene_viewport(self, scene_path: str, scene_name: str):
        """Open a scene and create a viewport for it"""
        # Open scene using editor session
        scene_doc = self.editor_session.open_scene(scene_path)
        if scene_doc:
            scene = scene_doc.get_scene()
            display_name = scene_doc.get_display_name()

            viewport = self.viewport_tabs.add_scene_viewport(display_name)
            # Delay rendering initialization to ensure widget is laid out
            from PyQt6.QtCore import QTimer
            QTimer.singleShot(0, lambda: viewport.initialize_rendering(self.editor_bridge, scene))
            self.scene_viewports[scene_path] = viewport

            # Connect viewport node selection to scene tree and inspector
            viewport.node_selected.connect(self._on_viewport_node_selected)

            # Set as active scene
            self.editor_session.set_active_scene(scene_doc.get_id())
            self.editor_bridge.set_active_scene(scene)

            # Update scene tree
            if 'scene_tree' in self.panels:
                self.panels['scene_tree'].set_scene(scene)

    def _on_node_deleted(self, node):
        """Handle node deletion - clear inspector if the deleted node was selected"""
        if 'inspector' in self.panels:
            # If the deleted node is currently selected, clear the inspector
            if self.panels['inspector'].current_node == node:
                self.panels['inspector'].set_node(None)

    def _on_scene_tree_node_selected(self, node):
        """Handle node selection from scene tree"""
        # Update viewport selection to show bounding box
        viewport = self.viewport_tabs.get_current_viewport()
        if viewport and viewport.editor_bridge and viewport.view_id:
            # Get all selected items from scene tree for multi-selection
            if 'scene_tree' in self.panels:
                selected_items = self.panels['scene_tree'].tree_widget.selectedItems()
                selected_nodes = []
                for item in selected_items:
                    node_uuid_str = item.data(0, Qt.ItemDataRole.UserRole)
                    if node_uuid_str:
                        try:
                            node_uuid = le.UUID.from_string(node_uuid_str)
                            selected_node = viewport.editor_bridge.get_node(node_uuid)
                            if selected_node:
                                selected_nodes.append(selected_node)
                        except:
                            pass
                
                # Update viewport's selected nodes list
                viewport.selected_nodes = selected_nodes
                
                # Set all selected nodes in editor bridge
                viewport.editor_bridge.set_selected_nodes(viewport.view_id, selected_nodes)
    
    def _on_viewport_node_selected(self, node):
        """Handle node selection from viewport picking"""
        # Update inspector
        if 'inspector' in self.panels:
            self.panels['inspector'].set_node(node)
        
        # Update scene tree selection
        if 'scene_tree' in self.panels:
            if node:
                self.panels['scene_tree'].select_node(node)
            else:
                self.panels['scene_tree'].clear_selection()

    def _on_scene_tab_changed(self, index: int):
        """Handle scene tab change"""
        # Clear inspector when switching tabs
        if 'inspector' in self.panels:
            self.panels['inspector'].set_node(None)

        # Handle no tabs open
        if index < 0:
            if 'scene_tree' in self.panels:
                self.panels['scene_tree'].set_scene(None)
            return

        viewport = self.viewport_tabs.widget(index)
        if viewport and hasattr(viewport, 'scene'):
            # Set the active scene in the editor bridge
            if viewport.scene:
                self.editor_bridge.set_active_scene(viewport.scene)

                # Find the scene document for this viewport and set it as active
                for path, vp in self.scene_viewports.items():
                    if vp == viewport:
                        scene_doc = self.editor_session.get_scene_document_by_path(path)
                        if scene_doc:
                            self.editor_session.set_active_scene(scene_doc.get_id())
                        break

                # Update scene tree panel
                if 'scene_tree' in self.panels:
                    self.panels['scene_tree'].set_scene(viewport.scene)

    def _on_gizmo_drag_ended(self):
        """Handle gizmo drag end - refresh inspector to show updated values"""
        if 'inspector' in self.panels:
            self.panels['inspector'].refresh_properties()

    def _on_scene_closed(self, scene_name: str):
        """Handle scene close"""
        # Remove from scene viewports mapping and close in editor session
        for path, viewport in list(self.scene_viewports.items()):
            if viewport.scene_name == scene_name:
                # Close the scene in editor session
                scene_doc = self.editor_session.get_scene_document_by_path(path)
                if scene_doc:
                    self.editor_session.close_scene(scene_doc.get_id())

                del self.scene_viewports[path]
                break

    def _on_save_scene_requested(self, tab_index: int):
        """Handle save request for a specific tab"""
        viewport = self.viewport_tabs.widget(tab_index)
        if not viewport or not hasattr(viewport, 'scene_name'):
            return

        # Find the scene document for this viewport
        for path, vp in self.scene_viewports.items():
            if vp == viewport:
                scene_doc = self.editor_session.get_scene_document_by_path(path)
                if scene_doc:
                    # Save the scene
                    if scene_doc.get_file_path():
                        if self.editor_session.save_scene(scene_doc.get_id()):
                            self.status_bar.showMessage(f"Saved scene: {scene_doc.get_display_name()}", 3000)
                            if 'console' in self.panels:
                                self.panels['console'].log_message(f"Saved scene: {scene_doc.get_display_name()}", "Info")
                        else:
                            QMessageBox.critical(self, "Save Failed", "Failed to save scene.")
                    else:
                        # No file path, show Save As dialog
                        self._save_scene_as_for_document(scene_doc)
                break

    def _on_close_scene_requested(self, tab_index: int):
        """Handle close request for a specific tab with dirty check"""
        viewport = self.viewport_tabs.widget(tab_index)
        if not viewport or not hasattr(viewport, 'scene_name'):
            return

        # Find the scene document for this viewport
        for path, vp in self.scene_viewports.items():
            if vp == viewport:
                scene_doc = self.editor_session.get_scene_document_by_path(path)
                if scene_doc and scene_doc.is_dirty():
                    # Scene is dirty, show confirmation dialog
                    reply = QMessageBox.question(
                        self,
                        "Unsaved Changes",
                        f"Scene '{scene_doc.get_display_name()}' has unsaved changes.\nDo you want to save before closing?",
                        QMessageBox.StandardButton.Save | QMessageBox.StandardButton.Discard | QMessageBox.StandardButton.Cancel,
                        QMessageBox.StandardButton.Save
                    )

                    if reply == QMessageBox.StandardButton.Save:
                        # Save the scene first
                        if scene_doc.get_file_path():
                            if not self.editor_session.save_scene(scene_doc.get_id()):
                                QMessageBox.critical(self, "Save Failed", "Failed to save scene. Close cancelled.")
                                return
                        else:
                            # No file path, show Save As dialog
                            if not self._save_scene_as_for_document(scene_doc):
                                return  # User cancelled Save As
                    elif reply == QMessageBox.StandardButton.Cancel:
                        return  # Don't close

                    # If Discard or successful Save, close the tab
                break

        # If this is the last tab, create a default scene BEFORE closing to preserve render context
        if self.viewport_tabs.count() == 1:
            self._create_default_scene()

        # Close the tab
        self.viewport_tabs.close_tab(tab_index)

    def _save_layout(self):
        """Save current layout to project settings"""
        layout_data = {
            "geometry": self.saveGeometry().toHex().data().decode(),
            "state": self.saveState().toHex().data().decode(),
            "panels": {}
        }
        
        # Save panel states
        for panel_id, panel in self.panels.items():
            layout_data["panels"][panel_id] = panel.get_state()
        
        # Save to project directory
        layout_file = Path(self.project.get_directory()) / ".lupine_layout.json"
        try:
            with open(layout_file, 'w') as f:
                json.dump(layout_data, f, indent=2)
            self.status_bar.showMessage("Layout saved", 3000)
        except Exception as e:
            QMessageBox.warning(self, "Save Layout Failed", f"Could not save layout:\n{str(e)}")
    
    def _load_layout(self):
        """Load layout from project settings"""
        layout_file = Path(self.project.get_directory()) / ".lupine_layout.json"
        
        if not layout_file.exists():
            return
        
        try:
            with open(layout_file, 'r') as f:
                layout_data = json.load(f)
            
            # Restore geometry and state
            if "geometry" in layout_data:
                self.restoreGeometry(bytes.fromhex(layout_data["geometry"]))
            if "state" in layout_data:
                self.restoreState(bytes.fromhex(layout_data["state"]))
            
            # Restore panel states
            if "panels" in layout_data:
                for panel_id, panel_state in layout_data["panels"].items():
                    if panel_id in self.panels:
                        self.panels[panel_id].set_state(panel_state)
            
            self.status_bar.showMessage("Layout loaded", 3000)
        except Exception as e:
            print(f"Failed to load layout: {e}")
    
    def _close_project(self):
        """Close the current project"""
        # Save layout before closing
        self._save_layout()
        
        # Emit signal to return to project manager
        self.project_closed.emit()
        self.close()
    
    def _open_project_settings(self):
        """Open project settings dialog"""
        dialog = ProjectSettingsDialog(self.project, self)
        dialog.settings_saved.connect(self._on_project_settings_saved)
        dialog.exec()
    
    def _open_input_map(self):
        """Open input map dialog"""
        dialog = InputMappingDialog(self.project.input_map, self)
        dialog.input_map_changed.connect(self._on_input_map_changed)
        dialog.exec()
    
    def _on_input_map_changed(self, input_map):
        """Handle input map changes"""
        self.project.input_map = input_map
        
        # Save the input map to project file
        ProjectFile.save_project(self.project)
        
        # Also save to a separate input_map.json file in project directory
        project_dir = Path(self.project.path).parent
        input_map_file = project_dir / "input_map.json"
        try:
            with open(input_map_file, 'w') as f:
                json.dump(input_map, f, indent=2)
            self.status_bar.showMessage("Input map saved", 3000)
        except Exception as e:
            QMessageBox.critical(self, "Error", f"Failed to save input map: {str(e)}")
    
    def _on_project_settings_saved(self, updated_project):
        """Handle project settings save"""
        self.project = updated_project
        self.setWindowTitle(f"Lupine Engine - {self.project.name}")
        self.status_bar.showMessage("Project settings saved", 3000)

        # Update editor bridge with new project settings
        if self.editor_bridge:
            self.editor_bridge.set_project_settings(
                self.project.window_width,
                self.project.window_height
            )

            # Apply texture filtering
            import lupine_engine as le
            texture_filtering_map = {
                "nearest": (le.FilterMode.Nearest, le.FilterMode.Nearest),
                "bilinear": (le.FilterMode.Linear, le.FilterMode.Linear),
                "cubic": (le.FilterMode.Linear, le.FilterMode.Linear)  # Cubic not yet implemented, use linear
            }
            min_filter, mag_filter = texture_filtering_map.get(self.project.texture_filtering.lower(), (le.FilterMode.Linear, le.FilterMode.Linear))
            self.editor_bridge.set_texture_filtering(min_filter, mag_filter)
    
    def _show_about(self):
        """Show about dialog"""
        QMessageBox.about(
            self,
            "About Lupine Engine",
            "<h3>Lupine Engine v0.1.0</h3>"
            "<p>A modern game engine with C++ backend and Python frontend.</p>"
            "<p>© 2024 Lupine Engine</p>"
        )

    def _show_coming_soon(self, tool_name: str):
        """Show coming soon dialog for unimplemented tools"""
        dialog = ComingSoonDialog(tool_name, self)
        dialog.exec()

    def _show_notepad(self):
        """Show and focus the Notepad tool"""
        if 'notepad' in self.panels:
            notepad = self.panels['notepad']
            if not notepad.isVisible():
                self.addDockWidget(Qt.DockWidgetArea.RightDockWidgetArea, notepad)
            notepad.setVisible(True)
            notepad.raise_()
            notepad.setFocus()
        else:
            self.panels['notepad'] = NotepadTool(self, self.project.get_directory())
            notepad = self.panels['notepad']
            self.addDockWidget(Qt.DockWidgetArea.RightDockWidgetArea, notepad)
            notepad.setVisible(True)
            notepad.raise_()
            notepad.setFocus()

    def _show_todo_list(self):
        """Show and focus the To Do List tool"""
        if 'todo_list' in self.panels:
            todo_list = self.panels['todo_list']
            if not todo_list.isVisible():
                self.addDockWidget(Qt.DockWidgetArea.RightDockWidgetArea, todo_list)
            todo_list.setVisible(True)
            todo_list.raise_()
            todo_list.setFocus()
        else:
            self.panels['todo_list'] = TodoListTool(self, self.project.get_directory())
            todo_list = self.panels['todo_list']
            self.addDockWidget(Qt.DockWidgetArea.RightDockWidgetArea, todo_list)
            todo_list.setVisible(True)
            todo_list.raise_()
            todo_list.setFocus()

    def _show_asset_manager(self):
        """Show and focus the Asset Manager tool"""
        if 'asset_manager' in self.panels:
            asset_manager = self.panels['asset_manager']
            if not asset_manager.isVisible():
                self.addDockWidget(Qt.DockWidgetArea.RightDockWidgetArea, asset_manager)
            asset_manager.setVisible(True)
            asset_manager.raise_()
            asset_manager.setFocus()
        else:
            self.panels['asset_manager'] = AssetManager(self, self.project.get_directory())
            asset_manager = self.panels['asset_manager']
            self.addDockWidget(Qt.DockWidgetArea.RightDockWidgetArea, asset_manager)
            asset_manager.setVisible(True)
            asset_manager.raise_()
            asset_manager.setFocus()

    def _show_feature_bug_tracker(self):
        """Show and focus the Feature Bug Tracker tool"""
        if 'feature_bug_tracker' in self.panels:
            tracker = self.panels['feature_bug_tracker']
            if not tracker.isVisible():
                self.addDockWidget(Qt.DockWidgetArea.RightDockWidgetArea, tracker)
            tracker.setVisible(True)
            tracker.raise_()
            tracker.setFocus()
        else:
            self.panels['feature_bug_tracker'] = FeatureBugTrackerTool(self, self.project.get_directory())
            tracker = self.panels['feature_bug_tracker']
            self.addDockWidget(Qt.DockWidgetArea.RightDockWidgetArea, tracker)
            tracker.setVisible(True)
            tracker.raise_()
            tracker.setFocus()

    def _show_globals_manager(self):
        """Show and focus the Globals Manager tool"""
        if 'globals_manager' in self.panels:
            globals_manager = self.panels['globals_manager']
            if not globals_manager.isVisible():
                self.addDockWidget(Qt.DockWidgetArea.RightDockWidgetArea, globals_manager)
            globals_manager.setVisible(True)
            globals_manager.raise_()
            globals_manager.setFocus()
        else:
            self.panels['globals_manager'] = GlobalsManagerTool(self, self.project.get_directory())
            globals_manager = self.panels['globals_manager']
            self.addDockWidget(Qt.DockWidgetArea.RightDockWidgetArea, globals_manager)
            globals_manager.setVisible(True)
            globals_manager.raise_()
            globals_manager.setFocus()

    def _show_sprite_animator(self):
        """Show and focus the Sprite Animator tool"""
        if 'sprite_animator' in self.panels:
            sprite_animator = self.panels['sprite_animator']
            if not sprite_animator.isVisible():
                self.addDockWidget(Qt.DockWidgetArea.RightDockWidgetArea, sprite_animator)
            sprite_animator.setVisible(True)
            sprite_animator.raise_()
            sprite_animator.setFocus()
        else:
            self.panels['sprite_animator'] = SpriteAnimatorTool(self, self.project.get_directory())
            sprite_animator = self.panels['sprite_animator']
            self.addDockWidget(Qt.DockWidgetArea.RightDockWidgetArea, sprite_animator)
            sprite_animator.setVisible(True)
            sprite_animator.raise_()
            sprite_animator.setFocus()

    def _show_tileset_builder(self):
        """Show and focus the Tileset 2D Builder tool"""
        if 'tileset_builder' in self.panels:
            tileset_builder = self.panels['tileset_builder']
            if not tileset_builder.isVisible():
                self.addDockWidget(Qt.DockWidgetArea.RightDockWidgetArea, tileset_builder)
            tileset_builder.setVisible(True)
            tileset_builder.raise_()
            tileset_builder.setFocus()
        else:
            self.panels['tileset_builder'] = Tileset2DBuilder(self, self.project.get_directory())
            tileset_builder = self.panels['tileset_builder']
            self.addDockWidget(Qt.DockWidgetArea.RightDockWidgetArea, tileset_builder)
            tileset_builder.setVisible(True)
            tileset_builder.raise_()
            tileset_builder.setFocus()

    def _show_tilemap_editor(self):
        """Show and focus the TileMap 2D Editor tool"""
        if 'tilemap_editor' in self.panels:
            tilemap_editor = self.panels['tilemap_editor']
            if not tilemap_editor.isVisible():
                self.addDockWidget(Qt.DockWidgetArea.RightDockWidgetArea, tilemap_editor)
            tilemap_editor.setVisible(True)
            tilemap_editor.raise_()
            tilemap_editor.setFocus()
        else:
            self.panels['tilemap_editor'] = TileMap2DEditor(self, self.project.get_directory())
            tilemap_editor = self.panels['tilemap_editor']
            self.addDockWidget(Qt.DockWidgetArea.RightDockWidgetArea, tilemap_editor)
            tilemap_editor.setVisible(True)
            tilemap_editor.raise_()
            tilemap_editor.setFocus()

    def _show_scribbler(self):
        """Show and focus the Scribbler tool"""
        if 'scribbler' in self.panels:
            scribbler = self.panels['scribbler']
            if not scribbler.isVisible():
                self.addDockWidget(Qt.DockWidgetArea.RightDockWidgetArea, scribbler)
            scribbler.setVisible(True)
            scribbler.raise_()
            scribbler.setFocus()
        else:
            self.panels['scribbler'] = ScribblerTool(self, self.project.get_directory())
            scribbler = self.panels['scribbler']
            self.addDockWidget(Qt.DockWidgetArea.RightDockWidgetArea, scribbler)
            scribbler.setVisible(True)
            scribbler.raise_()
            scribbler.setFocus()

    def _show_pixel_painter(self):
        """Show and focus the Pixel Painter tool"""
        if 'PixelPainterTool' in self.panels:
            pixel_painter = self.panels['PixelPainterTool']
            if not pixel_painter.isVisible():
                self.addDockWidget(Qt.DockWidgetArea.RightDockWidgetArea, pixel_painter)
            pixel_painter.setVisible(True)
            pixel_painter.raise_()
            pixel_painter.setFocus()
        else:
            self.panels['PixelPainterTool'] = PixelPainterTool(self, self.project.get_directory())
            pixel_painter = self.panels['PixelPainterTool']
            self.addDockWidget(Qt.DockWidgetArea.RightDockWidgetArea, pixel_painter)
            pixel_painter.setVisible(True)
            pixel_painter.raise_()
            pixel_painter.setFocus()

    def _show_voxel_builder(self):
        """Show and focus the Voxel Builder tool"""
        if 'VoxelBuilderTool' in self.panels:
            voxel_builder = self.panels['VoxelBuilderTool']
            if not voxel_builder.isVisible():
                self.addDockWidget(Qt.DockWidgetArea.RightDockWidgetArea, voxel_builder)
            voxel_builder.setVisible(True)
            voxel_builder.raise_()
            voxel_builder.setFocus()
        else:
            self.panels['VoxelBuilderTool'] = VoxelBuilderTool(self.editor_bridge, self, self.editor_session)
            voxel_builder = self.panels['VoxelBuilderTool']
            self.addDockWidget(Qt.DockWidgetArea.RightDockWidgetArea, voxel_builder)
            voxel_builder.setVisible(True)
            voxel_builder.raise_()
            voxel_builder.setFocus()

    def _new_scene(self):
        """Create a new scene"""
        # Create new scene using editor session
        scene_doc = self.editor_session.create_scene("New Scene")
        if scene_doc:
            scene = scene_doc.get_scene()
            scene_name = scene_doc.get_display_name()

            # Create viewport for the new scene
            viewport = self.viewport_tabs.add_scene_viewport(scene_name)
            # Delay rendering initialization to ensure widget is laid out
            from PyQt6.QtCore import QTimer
            QTimer.singleShot(0, lambda: viewport.initialize_rendering(self.editor_bridge, scene))
            self.scene_viewports[scene_name] = viewport

            # Set as active scene
            self.editor_session.set_active_scene(scene_doc.get_id())
            self.editor_bridge.set_active_scene(scene)

            # Update scene tree
            if 'scene_tree' in self.panels:
                self.panels['scene_tree'].set_scene(scene)

            self.status_bar.showMessage(f"Created new scene: {scene_name}", 3000)
            if 'console' in self.panels:
                self.panels['console'].log_message(f"Created new scene: {scene_name}", "Info")

    def _open_scene(self):
        """Open an existing scene from file"""
        # Show file dialog to select scene file
        scene_dir = Path(self.project.get_directory())
        file_path, _ = QFileDialog.getOpenFileName(
            self,
            "Open Scene",
            str(scene_dir),
            "Scene Files (*.scene);;All Files (*.*)"
        )

        if file_path:
            self._open_scene_as_tab(file_path)

    def _save_scene(self):
        """Save the current active scene"""
        # Get the current viewport's scene
        current_viewport = self.viewport_tabs.currentWidget()
        if not current_viewport or not hasattr(current_viewport, 'scene'):
            QMessageBox.warning(self, "No Scene", "No active scene to save.")
            return

        # Get active scene document
        scene_doc = self.editor_session.get_active_scene_document()
        if not scene_doc:
            QMessageBox.warning(self, "No Scene", "No active scene document.")
            return

        # Check if scene has a file path or is 'Untitled'
        if not scene_doc.get_file_path() or scene_doc.get_display_name() == "Untitled":
            # No file path or new scene, use Save As instead
            self._save_scene_as()
            return

        # Save the scene
        if self.editor_session.save_scene(scene_doc.get_id()):
            scene_name = scene_doc.get_display_name()
            self.status_bar.showMessage(f"Saved scene: {scene_name}", 3000)
            if 'console' in self.panels:
                self.panels['console'].log_message(f"Saved scene: {scene_name}", "Info")
        else:
            QMessageBox.critical(self, "Save Failed", "Failed to save scene.")

    def _save_scene_as(self):
        """Save the current active scene with a new file path"""
        # Get active scene document
        scene_doc = self.editor_session.get_active_scene_document()
        if not scene_doc:
            QMessageBox.warning(self, "No Scene", "No active scene document.")
            return

        # Show file dialog to select save location
        scene_dir = Path(self.project.get_directory())
        file_path, _ = QFileDialog.getSaveFileName(
            self,
            "Save Scene As",
            str(scene_dir / f"{scene_doc.get_display_name()}.scene"),
            "Scene Files (*.scene);;All Files (*.*)"
        )

        if file_path:
            # Ensure .scene extension
            if not file_path.endswith('.scene'):
                file_path += '.scene'

            # Save scene as
            if self.editor_session.save_scene_as(scene_doc.get_id(), file_path):
                scene_name = scene_doc.get_display_name()
                self.status_bar.showMessage(f"Saved scene as: {file_path}", 3000)
                if 'console' in self.panels:
                    self.panels['console'].log_message(f"Saved scene as: {file_path}", "Info")
            else:
                QMessageBox.critical(self, "Save Failed", "Failed to save scene.")

    def _save_scene_as_for_document(self, scene_doc):
        """Save a specific scene document with a new file path"""
        # Show file dialog to select save location
        scene_dir = Path(self.project.get_directory())
        file_path, _ = QFileDialog.getSaveFileName(
            self,
            "Save Scene As",
            str(scene_dir / f"{scene_doc.get_display_name()}.scene"),
            "Scene Files (*.scene);;All Files (*.*)"
        )

        if file_path:
            # Ensure .scene extension
            if not file_path.endswith('.scene'):
                file_path += '.scene'

            # Save scene as
            if self.editor_session.save_scene_as(scene_doc.get_id(), file_path):
                scene_name = scene_doc.get_display_name()
                self.status_bar.showMessage(f"Saved scene as: {file_path}", 3000)
                if 'console' in self.panels:
                    self.panels['console'].log_message(f"Saved scene as: {file_path}", "Info")
                return True
            else:
                QMessageBox.critical(self, "Save Failed", "Failed to save scene.")
                return False

        return False  # User cancelled

    def _save_all_scenes(self):
        """Save all open scenes and scripts"""
        scenes_saved = self.editor_session.save_all_scenes()

        # Also save all scripts
        if 'script_editor' in self.panels:
            self.panels['script_editor']._save_all_scripts()

        if scenes_saved:
            self.status_bar.showMessage("All scenes and scripts saved", 3000)
            if 'console' in self.panels:
                self.panels['console'].log_message("All scenes and scripts saved", "Info")
        else:
            QMessageBox.warning(self, "Save Failed", "Failed to save all scenes.")

    def _setup_file_watchers(self):
        """Setup file watchers for node, component, and prefab folders"""
        project_dir = Path(self.project.get_directory())

        # Watch node, component, and prefab directories
        watched_dirs = [
            project_dir / "node",
            project_dir / "component",
            project_dir / "prefab"
        ]

        for dir_path in watched_dirs:
            if dir_path.exists():
                self.file_watcher.addPath(str(dir_path))

        # Connect signals
        self.file_watcher.directoryChanged.connect(self._on_type_directory_changed)
        self.file_watcher.fileChanged.connect(self._on_type_file_changed)

    def _on_type_directory_changed(self, path):
        """Handle directory changes in watched folders"""
        # Rescan project types
        self.editor_bridge.scan_project_types(self.project.path)

        # Log the change
        if 'console' in self.panels:
            self.panels['console'].log_message(
                f"Type directory changed: {path}. Types reloaded.", "Info")

    def _on_type_file_changed(self, path):
        """Handle file changes in watched folders"""
        # Rescan project types
        self.editor_bridge.scan_project_types(self.project.path)

        # Log the change
        if 'console' in self.panels:
            self.panels['console'].log_message(
                f"Type file changed: {path}. Types reloaded.", "Info")

    def _on_play_game(self):
        """Handle Play Game button click"""
        # Auto-save if enabled
        if self.project.save_on_play:
            self._save_all_scenes()

        # Check if project has a main scene set
        if not self.project.main_scene:
            # Get current scene info
            current_viewport = self.viewport_tabs.currentWidget()
            has_open_scene = False
            current_scene_name = ""
            current_scene_path = None

            if current_viewport and hasattr(current_viewport, 'scene'):
                # Find the scene path for the current viewport
                for path, viewport in self.scene_viewports.items():
                    if viewport == current_viewport:
                        # Check if this is not the default "Untitled" scene
                        if path != "Untitled":
                            has_open_scene = True
                            current_scene_path = path
                            current_scene_name = Path(path).stem
                        break

            # Show custom dialog
            from dialogs import SetMainSceneDialog
            dialog = SetMainSceneDialog(has_open_scene, current_scene_name, self)
            result = dialog.exec()

            if result == QDialog.DialogCode.Accepted and dialog.should_set_as_main():
                # User wants to set current scene as main
                if current_scene_path:
                    # Convert absolute path to relative path from project directory
                    project_dir = Path(self.project.get_directory())
                    try:
                        relative_path = Path(current_scene_path).relative_to(project_dir)
                        self.project.main_scene = str(relative_path).replace('\\', '/')

                        # Save project
                        from project_file import ProjectFile
                        if ProjectFile.save_project(self.project):
                            self.status_bar.showMessage(f"Set main scene to: {relative_path}", 3000)
                            if 'console' in self.panels:
                                self.panels['console'].log_message(f"Set main scene to: {relative_path}", "Info")

                            # Now play the game
                            self._play_game_with_main_scene()
                        else:
                            QMessageBox.critical(self, "Error", "Failed to save project settings.")
                    except ValueError:
                        QMessageBox.critical(self, "Error", "Scene is not in the project directory.")
            return

        # Main scene is set, proceed with playing
        self._play_game_with_main_scene()

    def _play_game_with_main_scene(self):
        """Play the game using the main scene (assumes main scene is set)"""
        # Get project path
        project_path = self.project.path

        # Get project directory to construct full main scene path
        project_dir = Path(self.project.get_directory())
        main_scene_path = project_dir / self.project.main_scene

        # Check if main scene exists
        if not main_scene_path.exists():
            QMessageBox.warning(
                self,
                "Scene Not Found",
                f"Cannot play game: Main scene not found:\n{main_scene_path}\n\n"
                f"Please check project settings."
            )
            return

        # Play the game
        if self.runtime.play_game(
            project_path=project_path,
            window_width=self.project.window_width,
            window_height=self.project.window_height,
            debugging=True,
            blocking=False
        ):
            self.status_bar.showMessage("Game started", 3000)
            if 'console' in self.panels:
                self.panels['console'].log_message(f"Playing game: {self.project.name}", "Info")
        else:
            QMessageBox.critical(self, "Runtime Error", "Failed to start game runtime.")

    def _on_play_scene(self):
        """Handle Play Scene button click"""
        # Auto-save if enabled
        if self.project.save_on_play:
            self._save_all_scenes()

        # Get current active scene
        current_viewport = self.viewport_tabs.currentWidget()
        if not current_viewport or not hasattr(current_viewport, 'scene'):
            QMessageBox.warning(
                self,
                "No Scene to Play",
                "Cannot play scene: No scene is currently open.\n\n"
                "Please open or create a scene first."
            )
            return

        # Find the scene path for the current viewport
        scene_path = None
        for path, viewport in self.scene_viewports.items():
            if viewport == current_viewport:
                scene_path = path
                break

        # Check if this is the default "Untitled" scene (not saved)
        if not scene_path or scene_path == "Untitled":
            QMessageBox.warning(
                self,
                "No Scene to Play",
                "Cannot play scene: The current scene has not been saved.\n\n"
                "Please save the scene first (File > Save Scene)."
            )
            return

        # Get project path
        project_path = self.project.path

        # Play the scene
        if self.runtime.play_scene(
            project_path=project_path,
            scene_path=scene_path,
            window_width=self.project.window_width,
            window_height=self.project.window_height,
            debugging=True,
            blocking=False
        ):
            scene_name = Path(scene_path).stem
            self.status_bar.showMessage(f"Playing scene: {scene_name}", 3000)
            if 'console' in self.panels:
                self.panels['console'].log_message(f"Playing scene: {scene_path}", "Info")
        else:
            QMessageBox.critical(self, "Runtime Error", "Failed to start scene runtime.")

    def _on_pause(self):
        """Handle Pause button click (toggles between pause and resume)"""
        if not self.runtime.is_running():
            return

        if self.runtime.is_paused():
            # Resume
            self.runtime.resume()
            self.pause_btn.setText("⏸ Pause")
            self.status_bar.showMessage("Game resumed", 3000)
            if 'console' in self.panels:
                self.panels['console'].log_message("Game resumed", "Info")
        else:
            # Pause
            self.runtime.pause()
            self.pause_btn.setText("▶ Resume")
            self.status_bar.showMessage("Game paused", 3000)
            if 'console' in self.panels:
                self.panels['console'].log_message("Game paused", "Info")

    def _on_relaunch(self):
        """Handle Relaunch button click"""
        if self.runtime.relaunch(blocking=False):
            self.status_bar.showMessage("Game relaunched", 3000)
            if 'console' in self.panels:
                self.panels['console'].log_message("Game relaunched", "Info")
        else:
            QMessageBox.warning(
                self,
                "Relaunch Failed",
                "Cannot relaunch: No previous game session to relaunch."
            )

    def _on_stop(self):
        """Handle Stop button click"""
        if self.runtime.is_running():
            self.runtime.stop()
            self.status_bar.showMessage("Game stopped", 3000)
            if 'console' in self.panels:
                self.panels['console'].log_message("Game stopped", "Info")

    def _update_runtime_button_states(self):
        """Update button states based on runtime status"""
        is_running = self.runtime.is_running()
        is_paused = self.runtime.is_paused()
        has_previous_config = self.runtime._last_config is not None

        # Play buttons - always enabled (will stop and restart if already running)
        self.play_game_btn.setEnabled(True)
        self.play_scene_btn.setEnabled(True)

        # Relaunch - enabled when has previous config
        self.relaunch_btn.setEnabled(has_previous_config)

        # Pause - enabled when running
        self.pause_btn.setEnabled(is_running)
        if is_running and is_paused:
            self.pause_btn.setText("▶ Resume")
        else:
            self.pause_btn.setText("⏸ Pause")

        # Stop - enabled when running
        self.stop_btn.setEnabled(is_running)

    def eventFilter(self, obj, event):
        """Event filter to intercept Ctrl+Z/Ctrl+Shift+Z in text fields"""
        if event.type() == QEvent.Type.KeyPress:
            key_event = event
            # Check for Ctrl+Z (Undo)
            if key_event.key() == Qt.Key.Key_Z and key_event.modifiers() == Qt.KeyboardModifier.ControlModifier:
                # Check if focused widget is a text input or spinbox (but not script editor)
                focused = self.focusWidget()
                if isinstance(focused, (QLineEdit, QTextEdit, QPlainTextEdit, QSpinBox, QDoubleSpinBox)) and not hasattr(focused, 'custom_undo'):
                    # Intercept and trigger our undo instead
                    self._on_undo()
                    return True  # Event handled
            # Check for Ctrl+Shift+Z (Redo)
            elif key_event.key() == Qt.Key.Key_Z and key_event.modifiers() == (Qt.KeyboardModifier.ControlModifier | Qt.KeyboardModifier.ShiftModifier):
                # Check if focused widget is a text input or spinbox (but not script editor)
                focused = self.focusWidget()
                if isinstance(focused, (QLineEdit, QTextEdit, QPlainTextEdit, QSpinBox, QDoubleSpinBox)) and not hasattr(focused, 'custom_redo'):
                    # Intercept and trigger our redo instead
                    self._on_redo()
                    return True  # Event handled

        # Pass event to base class
        return super().eventFilter(obj, event)

    def _on_undo(self):
        """Handle undo action"""
        focused_widget = self.focusWidget()

        # Check if we're in a script editor (has custom_undo attribute)
        if hasattr(focused_widget, 'custom_undo'):
            # Use script editor's custom undo
            focused_widget.custom_undo()
            return

        # For all other cases (including inspector text fields), use scene editor undo
        # This ensures Ctrl+Z always undoes scene changes, not text field changes
        self.undo_manager.set_context("scene")
        # Save scroll position if scene tree is present
        scroll_value = None
        if 'scene_tree' in self.panels:
            tree_panel = self.panels['scene_tree']
            scroll_bar = tree_panel.tree_widget.verticalScrollBar()
            scroll_value = scroll_bar.value()
        if self.undo_manager.undo():
            self.status_bar.showMessage(f"Undone: {self.undo_manager.get_redo_description()}", 2000)
            # Refresh scene tree to show changes
            if 'scene_tree' in self.panels:
                tree_panel.refresh_tree()
                # Restore scroll position
                if scroll_value is not None:
                    tree_panel.tree_widget.verticalScrollBar().setValue(scroll_value)

    def _on_redo(self):
        """Handle redo action"""
        focused_widget = self.focusWidget()

        # Check if we're in a script editor (has custom_redo attribute)
        if hasattr(focused_widget, 'custom_redo'):
            # Use script editor's custom redo
            focused_widget.custom_redo()
            return

        # For all other cases (including inspector text fields), use scene editor redo
        # This ensures Ctrl+Shift+Z always redoes scene changes, not text field changes
        self.undo_manager.set_context("scene")
        # Save scroll position if scene tree is present
        scroll_value = None
        if 'scene_tree' in self.panels:
            tree_panel = self.panels['scene_tree']
            scroll_bar = tree_panel.tree_widget.verticalScrollBar()
            scroll_value = scroll_bar.value()
        if self.undo_manager.redo():
            self.status_bar.showMessage(f"Redone: {self.undo_manager.get_undo_description()}", 2000)
            # Refresh scene tree to show changes
            if 'scene_tree' in self.panels:
                tree_panel.refresh_tree()
                # Restore scroll position
                if scroll_value is not None:
                    tree_panel.tree_widget.verticalScrollBar().setValue(scroll_value)

    def _on_copy(self):
        """Context-aware copy handler"""
        focused_widget = self.focusWidget()

        # Check if we're in a text-editing context
        if hasattr(focused_widget, 'copy') and (
            hasattr(focused_widget, 'toPlainText') or  # QTextEdit
            hasattr(focused_widget, 'text') or         # QLineEdit
            hasattr(focused_widget, 'selectedText')    # Other text widgets
        ):
            # Use the widget's built-in copy
            focused_widget.copy()
        elif 'scene_tree' in self.panels:
            # Copy nodes from scene tree
            self.panels['scene_tree'].copy_selected_nodes()

    def _on_cut(self):
        """Context-aware cut handler"""
        focused_widget = self.focusWidget()

        # Check if we're in a text-editing context
        if hasattr(focused_widget, 'cut') and (
            hasattr(focused_widget, 'toPlainText') or  # QTextEdit
            hasattr(focused_widget, 'text') or         # QLineEdit
            hasattr(focused_widget, 'selectedText')    # Other text widgets
        ):
            # Use the widget's built-in cut
            focused_widget.cut()
        elif 'scene_tree' in self.panels:
            # Cut nodes from scene tree
            self.panels['scene_tree'].cut_selected_nodes()

    def _on_paste(self):
        """Context-aware paste handler"""
        focused_widget = self.focusWidget()

        # Check if we're in a text-editing context
        if hasattr(focused_widget, 'paste') and (
            hasattr(focused_widget, 'toPlainText') or  # QTextEdit
            hasattr(focused_widget, 'text') or         # QLineEdit
            hasattr(focused_widget, 'selectedText')    # Other text widgets
        ):
            # Use the widget's built-in paste
            focused_widget.paste()
        elif 'scene_tree' in self.panels:
            # Paste nodes to scene tree
            self.panels['scene_tree'].paste_nodes()

    def _on_delete(self):
        """Context-aware delete handler"""
        focused_widget = self.focusWidget()

        # Check if we're in a text-editing context with a delete/clear method
        if hasattr(focused_widget, 'del_') or hasattr(focused_widget, 'clear'):
            # For text widgets, delete selection
            if hasattr(focused_widget, 'textCursor'):
                cursor = focused_widget.textCursor()
                if cursor.hasSelection():
                    cursor.removeSelectedText()
        elif 'scene_tree' in self.panels:
            # Delete nodes from scene tree
            self.panels['scene_tree']._on_delete_node_clicked()

    def _on_duplicate(self):
        """Duplicate selected node(s)"""
        if 'scene_tree' in self.panels:
            self.panels['scene_tree']._on_duplicate_node_clicked()

    def closeEvent(self, event):
        """Handle window close event"""
        # Stop runtime if running
        if self.runtime.is_running():
            self.runtime.stop()

        # Save layout
        self._save_layout()

        # Confirm close
        reply = QMessageBox.question(
            self,
            "Close Editor",
            "Are you sure you want to close the editor?",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
        )

        if reply == QMessageBox.StandardButton.Yes:
            # Cleanup all viewports
            self.viewport_tabs.cleanup_all()

            # Shutdown editor bridge and session
            self.editor_bridge.shutdown()
            self.editor_session.shutdown()

            event.accept()
        else:
            event.ignore()
