"""
Lupine Engine Main Editor
The primary editor interface for managing projects and scenes
"""

from PyQt6.QtWidgets import (QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
                             QMenuBar, QMenu, QToolBar, QPushButton, QStatusBar,
                             QMessageBox, QTabWidget, QFileDialog, QLineEdit, QTextEdit, QPlainTextEdit,
                             QSpinBox, QDoubleSpinBox, QDockWidget, QApplication)
from PyQt6.QtCore import Qt, pyqtSignal, QTimer, QSize, QFileSystemWatcher, QEvent, QObject
from PyQt6.QtGui import QAction, QKeySequence, QIcon, QKeyEvent
from pathlib import Path
import json
import lupine_engine as le

from panels import (SceneTreePanel, AssetBrowserPanel, FileBrowserPanel,
                   InspectorPanel, ScriptEditorPanel, ShaderEditorPanel, ConsolePanel,
                   NodeSignalsPanel, InterfacePanel, AnimationTimelinePanel, BlendTreePanel,
                   ProfilerPanel)
from panels.base_panel import EditorPanel
from panels.inspector_panel import set_project_root as set_inspector_project_root
from tools import NotepadTool, TodoListTool, SpriteAnimatorTool, AssetManager, ScribblerTool, Tileset2DBuilder, TileMap2DEditor, TileMap25DEditor, PixelPainterTool, VoxelBuilderTool, FeatureBugTrackerTool, GlobalsManagerTool
from viewport_widget import ViewportTabWidget
from project_file import ProjectData, ProjectFile
from dialogs import ProjectSettingsDialog, InputMappingDialog, ComingSoonDialog, GodotSceneImportDialog, LocalizationEditorDialog, UIThemeEditorDialog, PluginsDialog
from plugin_system import PluginManager
from export import ExportDialog
from runtime_controller import RuntimeController
from undo_manager import UndoManager
from asset_file_watcher import AssetFileWatcher


class FloatingDockGuard(QObject):
    """
    Event filter that prevents floating dock widgets from docking into each other.

    This prevents a crash that occurs when floating dock widgets try to nest into
    each other due to the AllowNestedDocks option. The crash happens because Qt's
    internal dock widget management has issues with nested floating dock containers.

    The solution: When a dock becomes floating, temporarily set its allowed areas
    to NoDockWidgetArea to prevent it from being a dock target for other floating docks.
    """

    def __init__(self, main_window: QMainWindow):
        super().__init__(main_window)
        self._main_window = main_window
        self._floating_docks = set()  # Track which docks are currently floating
        self._original_areas = {}  # Store original allowed areas for each dock
        self._dock_ids = {}  # Map dock object id to dock for safe access

    def register_dock(self, dock: QDockWidget):
        """Register a dock widget to be monitored"""
        # Store the original allowed areas
        dock_id = id(dock)
        self._original_areas[dock_id] = dock.allowedAreas()
        self._dock_ids[dock_id] = dock

        # Connect to the topLevelChanged signal to track floating state
        # Store dock_id instead of dock reference to avoid prevent preventing GC
        dock.topLevelChanged.connect(lambda floating, did=dock_id: self._on_dock_floating_changed_safe(did, floating))

        # Clean up when dock is destroyed
        dock.destroyed.connect(lambda obj=None, did=dock_id: self._on_dock_destroyed(did))

    def _on_dock_destroyed(self, dock_id: int):
        """Clean up when a dock is destroyed"""
        self._floating_docks.discard(dock_id)
        self._original_areas.pop(dock_id, None)
        self._dock_ids.pop(dock_id, None)

    def _on_dock_floating_changed_safe(self, dock_id: int, floating: bool):
        """Handle when a dock widget becomes floating or docked (safe version using id)"""
        try:
            dock = self._dock_ids.get(dock_id)
            if dock is None:
                return

            # Check if the C++ object is still valid
            try:
                _ = dock.windowTitle()  # Access a property to verify object is valid
            except RuntimeError:
                # C++ object deleted
                self._on_dock_destroyed(dock_id)
                return

            if floating:
                # Dock became floating - add to tracking set
                self._floating_docks.add(dock_id)
                # When floating, set allowed areas to NoDockWidgetArea to prevent
                # other floating docks from docking into this one
                dock.setAllowedAreas(Qt.DockWidgetArea.NoDockWidgetArea)
            else:
                # Dock is no longer floating - restore allowed areas
                self._floating_docks.discard(dock_id)
                original = self._original_areas.get(dock_id, Qt.DockWidgetArea.AllDockWidgetAreas)
                dock.setAllowedAreas(original)
        except RuntimeError:
            # Dock widget was deleted
            self._on_dock_destroyed(dock_id)
        except Exception as e:
            print(f"FloatingDockGuard: Error in floating changed handler: {e}")


class MainEditor(QMainWindow):
    """Main editor window"""
    
    # Signals
    project_closed = pyqtSignal()
    
    def __init__(self, project: ProjectData, backend=None, parent=None):
        super().__init__(parent)
        self.project = project
        self.panels = {}

        # Initialize runtime controller
        self.runtime = RuntimeController()
        self.runtime.set_log_callback(self._on_runtime_log)

        # Initialize editor session and bridge
        self.editor_session = le.EditorSession()
        self.editor_session.initialize()

        # Open the project to initialize VFS and AssetDatabase
        project_path = Path(project.path) if isinstance(project.path, str) else project.path
        project_file_path = str(project_path / f"{project.name}.lupine")
        if not self.editor_session.open_project(project_file_path):
            # If project file doesn't exist, try alternative path formats
            alt_path = str(project_path)
            self.editor_session.open_project(alt_path)

        self.editor_bridge = self.editor_session.get_editor_bridge()

        # Use provided backend or default to OpenGL
        if backend is None:
            backend = le.GraphicsBackend.OpenGL
        self.graphics_backend = backend  # Store for passing to runtime
        self.editor_bridge.initialize(backend)

        # Restore the saved audio mixer (buses + DSP effect chains) into the engine
        # so they affect editor preview and are ready for the mixer panel.
        try:
            if getattr(self.project, 'audio_buses_state', None):
                self.editor_bridge.deserialize_audio_buses(json.dumps(self.project.audio_buses_state))
        except Exception as audio_restore_error:
            print(f"[AudioMixer] Failed to restore buses: {audio_restore_error}")

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
        # Timestamps of scene files the editor itself just wrote, keyed by
        # normalized path. Used to ignore the file watcher's echo of our own
        # saves so an editor save does not trigger an external-edit reload.
        self._scene_self_saves = {}

        # File watchers for auto-reloading types
        self.file_watcher = QFileSystemWatcher()
        self._setup_file_watchers()

        # Asset file watcher for automatic asset importing
        self.asset_file_watcher = None
        self._setup_asset_file_watcher()

        # Set project root for path conversion in inspector
        set_inspector_project_root(str(project.get_directory()))

        self.setWindowTitle(f"Lupine Engine - {project.name}")
        self.setMinimumSize(1280, 720)

        # Enable animated docks and nested docks for better UX
        # Note: AllowNestedDocks is needed for dock splitting within the main window,
        # but the FloatingDockGuard prevents floating docks from crashing when
        # they try to dock into each other.
        self.setDockOptions(
            QMainWindow.DockOption.AnimatedDocks |
            QMainWindow.DockOption.AllowNestedDocks |
            QMainWindow.DockOption.AllowTabbedDocks |
            QMainWindow.DockOption.GroupedDragging
        )

        # Install the floating dock guard to prevent crashes when floating
        # dock widgets try to dock into each other
        self._floating_dock_guard = FloatingDockGuard(self)
        self.installEventFilter(self._floating_dock_guard)

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
        self._setup_default_layout(show_message=False)
        self._load_layout()  # Load saved layout if exists
        self._load_main_scene()

        # Discover and load editor plugins from the project's plugins/ folder.
        # Plugins are loaded after panels and the main scene exist so they can
        # safely register docks, menu items and autoload singletons.
        # Discovery is cheap and UI-free so it runs now, but enabling plugins
        # adds dock widgets to the main window. A dock added while the window is
        # still hidden (the constructor runs before main.py calls show()) and
        # after restoreState() gets promoted by Qt to a floating top-level
        # window at screen origin with only its minimum size, which is why
        # plugin panels appeared in the top-left missing most of their content.
        # Defer enabling until the window is shown and its dock layout exists.
        self.plugin_manager = PluginManager(self)
        try:
            self.plugin_manager.discover()
        except Exception as e:
            print(f"Plugin system failed to discover plugins: {e}")
        QTimer.singleShot(0, self._load_enabled_plugins)
    
    def _load_enabled_plugins(self):
        """Enable persisted plugins once the window is shown and laid out.

        Runs from a zero-delay timer scheduled in the constructor so it fires
        on the first event-loop iteration, after main.py has shown the window.
        This guarantees plugin dock widgets attach to a visible, laid-out main
        window instead of floating off as undersized top-level windows.
        """
        try:
            self.plugin_manager.load_enabled()
        except Exception as e:
            print(f"Plugin system failed to load enabled plugins: {e}")

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
        save_scene_action.setShortcutContext(Qt.ShortcutContext.ApplicationShortcut)
        save_scene_action.triggered.connect(self._on_save)
        file_menu.addAction(save_scene_action)
        self.addAction(save_scene_action)

        save_scene_as_action = QAction("Save Scene As...", self)
        save_scene_as_action.setShortcut(QKeySequence("Ctrl+Shift+S"))
        save_scene_as_action.setShortcutContext(Qt.ShortcutContext.ApplicationShortcut)
        save_scene_as_action.triggered.connect(self._on_save_as)
        file_menu.addAction(save_scene_as_action)
        self.addAction(save_scene_as_action)

        save_all_action = QAction("Save All", self)
        save_all_action.setShortcut(QKeySequence("Ctrl+Alt+S"))
        save_all_action.setShortcutContext(Qt.ShortcutContext.ApplicationShortcut)
        save_all_action.triggered.connect(self._on_save_all)
        file_menu.addAction(save_all_action)
        self.addAction(save_all_action)

        file_menu.addSeparator()

        # Import submenu
        import_menu = file_menu.addMenu("Import")

        import_godot_scene_action = QAction("Godot Scene (.tscn)...", self)
        import_godot_scene_action.triggered.connect(self._import_godot_scene)
        import_menu.addAction(import_godot_scene_action)

        file_menu.addSeparator()

        export_project_action = QAction("Export Project...", self)
        export_project_action.setShortcut(QKeySequence("Ctrl+Shift+E"))
        export_project_action.triggered.connect(self._export_project)
        file_menu.addAction(export_project_action)

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

        self.view_node_signals_action = QAction("Node Signals", self)
        self.view_node_signals_action.setCheckable(True)
        self.view_node_signals_action.setChecked(True)
        self.view_node_signals_action.triggered.connect(lambda: self._toggle_panel('node_signals'))
        view_menu.addAction(self.view_node_signals_action)

        self.view_interface_action = QAction("Interfaces", self)
        self.view_interface_action.setCheckable(True)
        self.view_interface_action.setChecked(False)
        self.view_interface_action.triggered.connect(lambda: self._toggle_open_panel('interface'))
        view_menu.addAction(self.view_interface_action)

        self.view_script_editor_action = QAction("Script Editor", self)
        self.view_script_editor_action.setCheckable(True)
        self.view_script_editor_action.setChecked(True)
        self.view_script_editor_action.triggered.connect(lambda: self._toggle_panel('script_editor'))
        view_menu.addAction(self.view_script_editor_action)

        self.view_shader_editor_action = QAction("Shader Editor", self)
        self.view_shader_editor_action.setCheckable(True)
        self.view_shader_editor_action.setChecked(True)
        self.view_shader_editor_action.triggered.connect(lambda: self._toggle_panel('shader_editor'))
        view_menu.addAction(self.view_shader_editor_action)

        self.view_console_action = QAction("Console", self)
        self.view_console_action.setCheckable(True)
        self.view_console_action.setChecked(True)
        self.view_console_action.triggered.connect(lambda: self._toggle_panel('console'))
        view_menu.addAction(self.view_console_action)

        self.view_animation_timeline_action = QAction("Animation Timeline", self)
        self.view_animation_timeline_action.setCheckable(True)
        self.view_animation_timeline_action.triggered.connect(lambda: self._toggle_open_panel('animation_timeline'))
        view_menu.addAction(self.view_animation_timeline_action)

        self.view_blend_tree_action = QAction("Animation Tree", self)
        self.view_blend_tree_action.setCheckable(True)
        self.view_blend_tree_action.triggered.connect(lambda: self._toggle_open_panel('blend_tree'))
        view_menu.addAction(self.view_blend_tree_action)

        self.view_profiler_action = QAction("Profiler", self)
        self.view_profiler_action.setCheckable(True)
        self.view_profiler_action.setChecked(True)
        self.view_profiler_action.triggered.connect(lambda: self._toggle_panel('profiler'))
        view_menu.addAction(self.view_profiler_action)

        view_menu.addSeparator()
        
        reset_layout_action = QAction("Reset Layout", self)
        reset_layout_action.triggered.connect(self._setup_default_layout)
        view_menu.addAction(reset_layout_action)
        
        save_layout_action = QAction("Save Layout", self)
        save_layout_action.triggered.connect(self._save_layout)
        view_menu.addAction(save_layout_action)
        
        # Tools menu
        tools_menu = menubar.addMenu("Tools")
        self.tools_menu = tools_menu

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
        tilemap_25d_action.triggered.connect(self._show_tilemap_25d_editor)
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
        localization_action.triggered.connect(self._open_localization_editor)
        tools_menu.addAction(localization_action)

        # UI Theme (top-level)
        uitheme_action = QAction("UI Theme", self)
        uitheme_action.triggered.connect(self._open_uitheme_editor)
        tools_menu.addAction(uitheme_action)

        # Plugins (top-level)
        tools_menu.addSeparator()
        plugins_action = QAction("Plugins...", self)
        plugins_action.triggered.connect(self._show_plugins_dialog)
        tools_menu.addAction(plugins_action)
        tools_menu.addSeparator()

        # Audio submenu
        audio_menu = tools_menu.addMenu("Audio")

        mixer_action = QAction("Mixer", self)
        mixer_action.triggered.connect(self._show_audio_mixer)
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

        # Step button (advance one frame while paused)
        self.step_btn = QPushButton("⏭ Step")
        self.step_btn.setToolTip("Advance one frame (while paused)")
        self.step_btn.setEnabled(False)
        self.step_btn.setFixedHeight(28)
        self.step_btn.clicked.connect(self._on_step)
        toolbar.addWidget(self.step_btn)

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
        self.panels['node_signals'] = NodeSignalsPanel(self)
        self.panels['interface'] = InterfacePanel(self)
        self.panels['script_editor'] = ScriptEditorPanel(self)
        self.panels['shader_editor'] = ShaderEditorPanel(self)

        # Bottom panel
        self.panels['console'] = ConsolePanel(self)

        # Animation editors (opened on demand from the inspector / asset browser)
        self.panels['animation_timeline'] = AnimationTimelinePanel(self)
        self.panels['blend_tree'] = BlendTreePanel(self)

        # Profiler (bottom, tabbed with the console)
        self.panels['profiler'] = ProfilerPanel(self)

        # Register all panels with the floating dock guard and install event filters
        # This allows the guard to monitor dock widget events and prevent
        # floating docks from crashing when they try to dock into each other
        for panel in self.panels.values():
            self._floating_dock_guard.register_dock(panel)
            panel.installEventFilter(self._floating_dock_guard)

        # Initialize file browser with project root
        self.panels['file_browser'].set_project_root(self.project.path)

        # Initialize script editor with project root
        self.panels['script_editor'].project_root = self.project.get_directory()

        # Initialize shader editor with project root
        self.panels['shader_editor'].project_root = self.project.get_directory()

        # Initialize scene tree panel with editor bridge
        self.panels['scene_tree'].editor_bridge = self.editor_bridge

        # Initialize inspector panel with editor bridge
        self.panels['inspector'].editor_bridge = self.editor_bridge

        # Initialize node signals panel with editor bridge
        self.panels['node_signals'].editor_bridge = self.editor_bridge

        # Initialize interface panel with editor bridge
        self.panels['interface'].editor_bridge = self.editor_bridge
        self.panels['interface'].main_editor = self

        # Initialize animation editors with editor bridge
        self.panels['animation_timeline'].editor_bridge = self.editor_bridge
        self.panels['animation_timeline'].main_editor = self
        self.panels['blend_tree'].editor_bridge = self.editor_bridge
        self.panels['blend_tree'].main_editor = self

        # Initialize file browser panel with editor bridge (for archetype Create menu)
        self.panels['file_browser'].editor_bridge = self.editor_bridge
        self.panels['file_browser'].main_editor = self

        # Scan project types for node/component/prefab discovery
        self.editor_bridge.scan_project_types(self.project.path)

        # Connect scene tree to inspector. The inspector takes the full selection
        # list (shared multi-node editing); the single-node panels take the primary.
        self.panels['scene_tree'].nodes_selected.connect(self.panels['inspector'].set_nodes)
        self.panels['scene_tree'].node_selected.connect(self.panels['node_signals'].set_node)
        self.panels['scene_tree'].node_selected.connect(self.panels['animation_timeline'].set_node)
        self.panels['scene_tree'].node_selected.connect(self.panels['blend_tree'].set_node)
        self.panels['scene_tree'].node_selected.connect(self._on_scene_tree_node_selected)
        self.panels['scene_tree'].node_deleted.connect(self._on_node_deleted)

        # Connect file browser signals
        self.panels['file_browser'].script_opened.connect(self._open_script_in_editor)
        self.panels['file_browser'].shader_opened.connect(self._open_shader_in_editor)
        self.panels['file_browser'].scene_opened.connect(self._open_scene_as_tab)
        self.panels['file_browser'].prefab_opened.connect(self._open_prefab_as_tab)
        self.panels['file_browser'].archetype_instance_opened.connect(
            self.panels['inspector'].set_archetype_instance)
        self.panels['file_browser'].archetype_definition_opened.connect(
            self.open_archetype_definition)
        self.panels['file_browser'].animation_clip_opened.connect(self._open_animation_clip)
        self.panels['file_browser'].animation_graph_opened.connect(self._open_animation_graph)
        self.panels['file_browser'].theme_opened.connect(self._open_uitheme_editor_from_path)

        # Connect script editor saved signal to refresh script components in inspector
        self.panels['script_editor'].script_saved.connect(self._on_script_saved)

        # Connect shader editor saved signal to recompile attached shaders and refresh params
        self.panels['shader_editor'].shader_saved.connect(self._on_shader_saved)

        # Connect panel visibility changes to menu actions
        self.panels['scene_tree'].visibilityChanged.connect(
            lambda visible: self.view_scene_tree_action.setChecked(visible))
        self.panels['asset_browser'].visibilityChanged.connect(
            lambda visible: self.view_asset_browser_action.setChecked(visible))
        self.panels['file_browser'].visibilityChanged.connect(
            lambda visible: self.view_file_browser_action.setChecked(visible))
        self.panels['inspector'].visibilityChanged.connect(
            lambda visible: self.view_inspector_action.setChecked(visible))
        self.panels['node_signals'].visibilityChanged.connect(
            lambda visible: self.view_node_signals_action.setChecked(visible))
        self.panels['interface'].visibilityChanged.connect(
            lambda visible: self.view_interface_action.setChecked(visible))
        self.panels['script_editor'].visibilityChanged.connect(
            lambda visible: self.view_script_editor_action.setChecked(visible))
        self.panels['shader_editor'].visibilityChanged.connect(
            lambda visible: self.view_shader_editor_action.setChecked(visible))
        self.panels['console'].visibilityChanged.connect(
            lambda visible: self.view_console_action.setChecked(visible))
        self.panels['animation_timeline'].visibilityChanged.connect(
            lambda visible: self.view_animation_timeline_action.setChecked(visible))
        self.panels['blend_tree'].visibilityChanged.connect(
            lambda visible: self.view_blend_tree_action.setChecked(visible))
        self.panels['profiler'].visibilityChanged.connect(
            lambda visible: self.view_profiler_action.setChecked(visible))

        # Dock the animation editors at the bottom, tabbed with the console, hidden
        # until opened from the inspector or the asset browser.
        from PyQt6.QtCore import Qt as _Qt
        self.addDockWidget(_Qt.DockWidgetArea.BottomDockWidgetArea, self.panels['animation_timeline'])
        self.addDockWidget(_Qt.DockWidgetArea.BottomDockWidgetArea, self.panels['blend_tree'])
        self.addDockWidget(_Qt.DockWidgetArea.BottomDockWidgetArea, self.panels['profiler'])
        self.addDockWidget(_Qt.DockWidgetArea.RightDockWidgetArea, self.panels['interface'])
        try:
            self.tabifyDockWidget(self.panels['console'], self.panels['animation_timeline'])
            self.tabifyDockWidget(self.panels['animation_timeline'], self.panels['blend_tree'])
            self.tabifyDockWidget(self.panels['console'], self.panels['profiler'])
            self.tabifyDockWidget(self.panels['node_signals'], self.panels['interface'])
        except Exception:
            pass
        self.panels['animation_timeline'].setVisible(False)
        self.panels['blend_tree'].setVisible(False)
        self.panels['interface'].setVisible(False)

    def _open_panel(self, panel_id: str):
        """Ensure a panel is docked and visible (used by the animation editors)."""
        if panel_id not in self.panels:
            return
        from PyQt6.QtCore import Qt as _Qt
        panel = self.panels[panel_id]
        if self.dockWidgetArea(panel) == _Qt.DockWidgetArea.NoDockWidgetArea and not panel.isFloating():
            self.addDockWidget(_Qt.DockWidgetArea.BottomDockWidgetArea, panel)
        panel.setVisible(True)
        panel.raise_()

    def _toggle_open_panel(self, panel_id: str):
        """Toggle an on-demand panel, re-docking it when shown."""
        panel = self.panels.get(panel_id)
        if not panel:
            return
        if panel.isVisible():
            panel.setVisible(False)
        else:
            self._open_panel(panel_id)

    def _open_animation_clip(self, path):
        self._open_panel('animation_timeline')
        self.panels['animation_timeline'].load_clip_file(path)

    def _open_animation_graph(self, path):
        self._open_panel('blend_tree')
        self.panels['blend_tree'].load_graph_file(path)

    def _register_panel(self, panel_id: str, panel):
        """Register a new panel with the floating dock guard"""
        self.panels[panel_id] = panel
        self._floating_dock_guard.register_dock(panel)
        panel.installEventFilter(self._floating_dock_guard)

    def _toggle_panel(self, panel_id: str):
        """Toggle panel visibility"""
        if panel_id in self.panels:
            panel = self.panels[panel_id]
            panel.setVisible(not panel.isVisible())
    
    def _setup_default_layout(self, show_message: bool = True):
        """Setup the default panel layout"""
        # Core panels that should be in the default layout
        core_panels = ['scene_tree', 'asset_browser', 'file_browser', 'inspector', 'node_signals', 'script_editor', 'shader_editor', 'console', 'profiler']

        # First, ensure all panels are not floating and remove them
        for panel_id, panel in self.panels.items():
            # Stop floating first to ensure proper state
            if panel.isFloating():
                panel.setFloating(False)
            self.removeDockWidget(panel)

            # Hide tool panels (non-core panels) - they should be opened manually
            if panel_id not in core_panels:
                panel.setVisible(False)

        # Left side - Scene Tree, Asset Browser, File Browser, Shader Editor (stacked as tabs)
        self.addDockWidget(Qt.DockWidgetArea.LeftDockWidgetArea, self.panels['scene_tree'])
        self.addDockWidget(Qt.DockWidgetArea.LeftDockWidgetArea, self.panels['asset_browser'])
        self.addDockWidget(Qt.DockWidgetArea.LeftDockWidgetArea, self.panels['file_browser'])
        self.addDockWidget(Qt.DockWidgetArea.LeftDockWidgetArea, self.panels['shader_editor'])
        self.tabifyDockWidget(self.panels['scene_tree'], self.panels['asset_browser'])
        self.tabifyDockWidget(self.panels['asset_browser'], self.panels['file_browser'])
        self.tabifyDockWidget(self.panels['file_browser'], self.panels['shader_editor'])

        # Right side - Inspector, Node Signals, Script Editor (stacked as tabs)
        self.addDockWidget(Qt.DockWidgetArea.RightDockWidgetArea, self.panels['inspector'])
        self.addDockWidget(Qt.DockWidgetArea.RightDockWidgetArea, self.panels['node_signals'])
        self.addDockWidget(Qt.DockWidgetArea.RightDockWidgetArea, self.panels['script_editor'])
        self.tabifyDockWidget(self.panels['inspector'], self.panels['node_signals'])
        self.tabifyDockWidget(self.panels['node_signals'], self.panels['script_editor'])

        # Bottom - Console + Profiler (tabbed)
        self.addDockWidget(Qt.DockWidgetArea.BottomDockWidgetArea, self.panels['console'])
        self.addDockWidget(Qt.DockWidgetArea.BottomDockWidgetArea, self.panels['profiler'])
        self.tabifyDockWidget(self.panels['console'], self.panels['profiler'])
        self.panels['console'].raise_()

        # Make sure core panels are visible and not floating
        for panel_id in core_panels:
            if panel_id in self.panels:
                panel = self.panels[panel_id]
                panel.setFloating(False)
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

        if show_message:
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
    
    def _open_shader_in_editor(self, shader_path: str):
        """Open a .lsh shader file in the shader editor panel"""
        self.status_bar.showMessage(f"Opening shader: {shader_path}", 3000)

        if 'shader_editor' in self.panels:
            self.panels['shader_editor'].open_shader(shader_path)
            self.panels['shader_editor'].raise_()
            self.panels['shader_editor'].setVisible(True)

        if 'console' in self.panels:
            self.panels['console'].log_message(f"Shader opened: {shader_path}", "Info")

    def _on_runtime_log(self, message: str, level: str):
        """Mirror a runtime/script log line into the editor Console panel."""
        if 'console' not in self.panels:
            return
        level_map = {
            'trace': 'Debug', 'debug': 'Debug', 'info': 'Info',
            'warning': 'Warning', 'warn': 'Warning',
            'error': 'Error', 'critical': 'Error',
        }
        self.panels['console'].log_message(message, level_map.get(str(level).lower(), 'Info'))

    def _resolve_project_path(self, path: str) -> str:
        """Resolve a res:// path to an absolute filesystem path within the project."""
        if path.startswith("res://"):
            relative = path[len("res://"):]
            return str(Path(self.project.get_directory()) / relative)
        return path

    def open_archetype_definition(self, definition_path: str):
        """Open an archetype definition: schema editor for .archetype, script editor for scripts."""
        abs_path = self._resolve_project_path(definition_path)
        suffix = Path(abs_path).suffix.lower()
        if suffix == '.archetype':
            self._open_archetype_schema_editor(abs_path)
        else:
            self._open_script_in_editor(abs_path)

    def _open_archetype_schema_editor(self, abs_path: str):
        """Open the visual schema editor for an existing .archetype definition."""
        from PyQt6.QtWidgets import QDialog
        from dialogs import ArchetypeDefinitionDialog

        schema = None
        try:
            loaded = self.editor_bridge.load_archetype_definition_file(abs_path)
            if loaded:
                schema = json.loads(loaded)
        except Exception:
            schema = None

        dialog = ArchetypeDefinitionDialog(self, existing_path=abs_path, schema=schema,
                                           editor_bridge=self.editor_bridge)
        if dialog.exec() != QDialog.DialogCode.Accepted:
            return

        save_path = dialog.get_save_path()
        new_schema = dialog.get_schema_json()
        try:
            self.editor_bridge.create_archetype_definition(save_path, json.dumps(new_schema))
        except Exception as e:
            if 'console' in self.panels:
                self.panels['console'].log_message(
                    f"Failed to save archetype definition: {str(e)}", "Error")
            return

        if 'file_browser' in self.panels:
            self.panels['file_browser']._refresh_tree()
        if ('inspector' in self.panels and
                self.panels['inspector'].current_archetype_path):
            self.panels['inspector']._refresh_archetype()

    def _on_shader_saved(self, shader_path: str):
        """Refresh attached-shader inspectors when a .lsh is saved.

        The engine recompiles attached shaders automatically on the next frame (it tracks
        each .lsh file's modification time), so no explicit cache invalidation is needed here.
        We only re-introspect the parameter list in the inspector in case uniforms changed.
        """
        if 'inspector' in self.panels and hasattr(self.panels['inspector'], 'on_shader_saved'):
            self.panels['inspector'].on_shader_saved(shader_path)

        if 'console' in self.panels:
            self.panels['console'].log_message(f"Shader saved: {shader_path}", "Info")

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
        # Preserve the outgoing scene's edits before switching active scene
        self._sync_scene_dirty()
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
        # Preserve the outgoing scene's edits before switching active scene
        self._sync_scene_dirty()
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

    def _open_prefab_as_tab(self, prefab_path: str):
        """Open a .prefab file as a new editable viewport tab"""
        prefab_name = Path(prefab_path).stem

        # Check if the prefab is already open
        if prefab_path in self.scene_viewports:
            viewport = self.scene_viewports[prefab_path]
            for i in range(self.viewport_tabs.count()):
                if self.viewport_tabs.widget(i) == viewport:
                    self.viewport_tabs.setCurrentIndex(i)
                    self.status_bar.showMessage(f"Switched to prefab: {prefab_name}", 3000)
                    return

        self._open_prefab_viewport(prefab_path, prefab_name)
        self.status_bar.showMessage(f"Opened prefab: {prefab_name}", 3000)

        if 'console' in self.panels:
            self.panels['console'].log_message(f"Prefab opened for editing: {prefab_path}", "Info")

    def _open_prefab_viewport(self, prefab_path: str, prefab_name: str):
        """Open a prefab and create an editable viewport for it"""
        # open_prefab is added by the prefab-editing C++ build.
        if not hasattr(self.editor_session, "open_prefab"):
            QMessageBox.information(
                self, "Rebuild Required",
                "Editing prefabs requires the engine to be rebuilt with prefab-editing "
                "support. Please rebuild the engine/runtime module.")
            return

        # Preserve the outgoing scene's edits before switching active scene
        self._sync_scene_dirty()

        # Open prefab using editor session (loads it into an editable single-root scene)
        scene_doc = self.editor_session.open_prefab(prefab_path)
        if not scene_doc:
            QMessageBox.critical(self, "Open Failed",
                                 f"Failed to open prefab for editing:\n{prefab_path}")
            return

        scene = scene_doc.get_scene()
        display_name = scene_doc.get_display_name()

        viewport = self.viewport_tabs.add_scene_viewport(display_name)
        from PyQt6.QtCore import QTimer
        QTimer.singleShot(0, lambda: viewport.initialize_rendering(self.editor_bridge, scene))
        self.scene_viewports[prefab_path] = viewport

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
        # Update inspector. Honour a viewport multi-selection (Ctrl/Shift pick) so
        # shared editing works from the viewport as well as the scene tree.
        if 'inspector' in self.panels:
            viewport = self.viewport_tabs.get_current_viewport()
            selected_nodes = list(getattr(viewport, 'selected_nodes', []) or []) if viewport else []
            if len(selected_nodes) > 1:
                self.panels['inspector'].set_nodes(selected_nodes)
            else:
                self.panels['inspector'].set_node(node)

        # Update node signals panel
        if 'node_signals' in self.panels:
            self.panels['node_signals'].set_node(node)

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
        if 'node_signals' in self.panels:
            self.panels['node_signals'].set_node(None)

        # Handle no tabs open
        if index < 0:
            if 'scene_tree' in self.panels:
                self.panels['scene_tree'].set_scene(None)
            return

        viewport = self.viewport_tabs.widget(index)
        if viewport and hasattr(viewport, 'scene'):
            # Set the active scene in the editor bridge
            if viewport.scene:
                # Capture the outgoing scene's edits before the switch resets the flag
                self._sync_scene_dirty()
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
        # A gizmo drag edits the scene through the C++ command system, which only
        # raises the bridge dirty flag; fold it into the active document now.
        self._sync_scene_dirty()
        if 'inspector' in self.panels:
            # Auto hot-reload: push the dragged node's transform to running
            # instances. Do this before refresh_properties() rebuilds the widgets.
            self.panels['inspector'].push_transform_live()
            self.panels['inspector'].refresh_properties()

    def _on_script_saved(self, script_path: str):
        """Handle script save - reload script components using this script and refresh inspector"""
        if 'console' in self.panels:
            self.panels['console'].log_message(f"[ScriptReload] Script saved signal received: {script_path}", "Info")

        # Reload any script components using this script path in the current scene
        reloaded_count = self._reload_script_components_for_path(script_path)

        # Auto hot-reload: push a script reload into any running play instances.
        if self.runtime.is_running():
            self.runtime.push_reload_scripts()

        if 'console' in self.panels:
            self.panels['console'].log_message(f"[ScriptReload] Reloaded {reloaded_count} script component(s)", "Info")

        # Refresh the inspector to show updated export properties
        if 'inspector' in self.panels:
            self.panels['console'].log_message(f"[ScriptReload] Calling inspector.refresh_properties()", "Debug")
            self.panels['inspector'].refresh_properties()

    def _reload_script_components_for_path(self, script_path: str) -> int:
        """Reload all script components in the active scene that use the given script path.
        Returns the number of components reloaded."""
        if not self.editor_bridge:
            if 'console' in self.panels:
                self.panels['console'].log_message(f"[ScriptReload] No editor_bridge!", "Warning")
            return 0

        # Normalize the path for comparison (case-insensitive on Windows)
        import os
        normalized_path = os.path.normcase(os.path.normpath(os.path.abspath(script_path)))

        if 'console' in self.panels:
            self.panels['console'].log_message(f"[ScriptReload] Normalized saved script path: {normalized_path}", "Debug")

        # Get the active scene's root node
        root = self.editor_bridge.get_root_node()
        if not root:
            if 'console' in self.panels:
                self.panels['console'].log_message(f"[ScriptReload] No root node in scene!", "Warning")
            return 0

        if 'console' in self.panels:
            self.panels['console'].log_message(f"[ScriptReload] Searching from root: {root.get_name()}", "Debug")

        # Recursively find and reload script components
        return self._reload_script_components_recursive(root, normalized_path)

    def _resolve_res_path(self, res_path: str) -> str:
        """Resolve a res:// path to an absolute path"""
        import os
        if not res_path:
            return res_path

        if res_path.startswith("res://"):
            # Try to resolve via AssetDatabase
            try:
                import lupine_engine as le
                asset_db = le.AssetDatabase.get_instance()
                if asset_db.is_initialized():
                    resolved = asset_db.resolve_asset(res_path)
                    if resolved:
                        return resolved
            except Exception:
                pass

            # Fallback: manual resolution using project root
            try:
                from panels.inspector_panel import get_project_root
                project_root = get_project_root()
                if project_root:
                    relative_path = res_path[6:]  # Remove "res://"
                    return os.path.join(project_root, relative_path)
            except Exception:
                pass

        return res_path

    def _reload_script_components_recursive(self, node, script_path: str) -> int:
        """Recursively reload script components in a node and its children.
        Returns the number of components reloaded."""
        if not node:
            return 0

        reloaded_count = 0

        # Check all components of this node
        components = node.get_components()

        for component in components:
            type_name = component.get_type_name()
            if type_name in ('LuaScriptComponent', 'PythonScriptComponent', 'MRubyScriptComponent'):
                # Get the script path of this component
                import json
                import os
                props_json = self.editor_bridge.get_component_properties(component)
                if props_json:
                    try:
                        props = json.loads(props_json)
                        # script_path can be in properties or at root level (depending on serialization method)
                        comp_script_path = props.get('properties', {}).get('script_path', '')
                        if not comp_script_path:
                            comp_script_path = props.get('script_path', '')

                        # Resolve res:// path to absolute path
                        if comp_script_path.startswith("res://"):
                            comp_script_path = self._resolve_res_path(comp_script_path)

                        # Normalize for comparison (case-insensitive on Windows)
                        normalized_comp_path = os.path.normcase(os.path.normpath(os.path.abspath(comp_script_path))) if comp_script_path else ''

                        if 'console' in self.panels:
                            self.panels['console'].log_message(
                                f"[ScriptReload] Comparing:\n  Component: {normalized_comp_path}\n  Saved:     {script_path}", "Debug")

                        if normalized_comp_path == script_path:
                            # Reload this script component
                            if 'console' in self.panels:
                                self.panels['console'].log_message(
                                    f"[ScriptReload] MATCH! Reloading component...", "Info")
                            result = self.editor_bridge.reload_script_component(component)
                            if result:
                                reloaded_count += 1
                                if 'console' in self.panels:
                                    self.panels['console'].log_message(
                                        f"[ScriptReload] Successfully reloaded: {component.get_name()}", "Info")
                            else:
                                if 'console' in self.panels:
                                    self.panels['console'].log_message(
                                        f"[ScriptReload] reload_script_component returned False!", "Error")
                    except json.JSONDecodeError as e:
                        if 'console' in self.panels:
                            self.panels['console'].log_message(
                                f"[ScriptReload] JSON decode error: {e}", "Error")

        # Recurse into children
        for child in node.get_children():
            reloaded_count += self._reload_script_components_recursive(child, script_path)

        return reloaded_count

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

        # Capture the active scene's edits into its document before saving a
        # specific tab, so saving a background tab cannot drop the live flag.
        self._sync_scene_dirty()

        # Find the scene document for this viewport
        for path, vp in self.scene_viewports.items():
            if vp == viewport:
                scene_doc = self.editor_session.get_scene_document_by_path(path)
                if scene_doc:
                    # Save the scene
                    if scene_doc.get_file_path():
                        if self.editor_session.save_scene(scene_doc.get_id()):
                            self._note_scene_self_save(scene_doc.get_file_path())
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

        # Make sure the active scene's latest edits are reflected before checking
        self._sync_scene_dirty()

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
                            self._note_scene_self_save(scene_doc.get_file_path())
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

    def _open_localization_editor(self):
        """Open the localization editor dialog"""
        dialog = LocalizationEditorDialog(self.project, self.editor_bridge, self)
        dialog.localization_changed.connect(self._on_localization_changed)
        dialog.exec()

    def _on_localization_changed(self, config):
        """Persist localization config changes into the project file.

        The dialog already writes localization.json and the table files; here we
        keep the in-memory project + .lupine project file in sync.
        """
        self.project.localization = config
        ProjectFile.save_project(self.project)
        self.status_bar.showMessage("Localization saved", 3000)

    def _open_uitheme_editor(self):
        """Open the UI Theme editor window."""
        dialog = UIThemeEditorDialog(self.project, self.editor_bridge, self,
                                     editor_session=self.editor_session)
        dialog.theme_changed.connect(self._on_uitheme_changed)
        dialog.exec()

    def _open_uitheme_editor_from_path(self, path):
        """Open the UI Theme editor focused on a specific .uitheme / .palette file."""
        dialog = UIThemeEditorDialog(self.project, self.editor_bridge, self, theme_path=path,
                                     editor_session=self.editor_session)
        dialog.theme_changed.connect(self._on_uitheme_changed)
        dialog.exec()

    def _on_uitheme_changed(self, path):
        """The dialog already wrote the theme files and called reload_theme(); just
        refresh the file browser so any newly created assets appear."""
        if 'file_browser' in self.panels:
            self.panels['file_browser']._refresh_tree()
        self.status_bar.showMessage("UI theme saved", 3000)

    def _import_godot_scene(self):
        """Open Godot scene import dialog"""
        project_path = Path(self.project.path).parent
        result_path = GodotSceneImportDialog.import_godot_scene(
            self.editor_bridge,
            project_path,
            self
        )

        if result_path:
            # Open the imported scene
            self._open_scene_as_tab(result_path)
            self.status_bar.showMessage(f"Imported Godot scene to: {result_path}", 5000)

            # Refresh the asset browser to show new files
            if 'asset_browser' in self.panels:
                # self.panels['asset_browser'].refresh()
                pass  # TODO: implement asset browser refresh
            if 'file_browser' in self.panels:
                self.panels['file_browser']._refresh_tree()

    def _export_project(self):
        """Open export project dialog"""
        # Load project data as dict for export dialog
        try:
            with open(self.project.path, 'r') as f:
                project_data = json.load(f)
        except Exception as e:
            QMessageBox.critical(self, "Error", f"Failed to load project data: {str(e)}")
            return

        dialog = ExportDialog(self.project.path, project_data, self)
        dialog.exec()

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

    def _show_audio_mixer(self):
        """Show and focus the Audio Mixer panel"""
        if 'audio_mixer' not in self.panels:
            from panels.audio_mixer_panel import AudioMixerPanel
            mixer = AudioMixerPanel(self)
            mixer.editor_bridge = self.editor_bridge
            mixer.main_editor = self
            self._register_panel('audio_mixer', mixer)
            self.addDockWidget(Qt.DockWidgetArea.BottomDockWidgetArea, mixer)
        mixer = self.panels['audio_mixer']
        mixer.editor_bridge = self.editor_bridge
        mixer.main_editor = self
        if not mixer.isVisible():
            self.addDockWidget(Qt.DockWidgetArea.BottomDockWidgetArea, mixer)
        mixer.setVisible(True)
        mixer.raise_()
        mixer.refresh()
        mixer.setFocus()

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
            notepad = NotepadTool(self, self.project.get_directory())
            self._register_panel('notepad', notepad)
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
            todo_list = TodoListTool(self, self.project.get_directory())
            self._register_panel('todo_list', todo_list)
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
            asset_manager = AssetManager(self, self.project.get_directory())
            self._register_panel('asset_manager', asset_manager)
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
            tracker = FeatureBugTrackerTool(self, self.project.get_directory())
            self._register_panel('feature_bug_tracker', tracker)
            self.addDockWidget(Qt.DockWidgetArea.RightDockWidgetArea, tracker)
            tracker.setVisible(True)
            tracker.raise_()
            tracker.setFocus()

    def _show_plugins_dialog(self):
        """Open the plugin management dialog."""
        if not hasattr(self, 'plugin_manager') or self.plugin_manager is None:
            return
        self.plugin_manager.discover()
        dialog = PluginsDialog(self.plugin_manager, self)
        dialog.exec()

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
            globals_manager = GlobalsManagerTool(self, self.project.get_directory())
            self._register_panel('globals_manager', globals_manager)
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
            sprite_animator = SpriteAnimatorTool(self, self.project.get_directory())
            self._register_panel('sprite_animator', sprite_animator)
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
            tileset_builder = Tileset2DBuilder(self, self.project.get_directory())
            self._register_panel('tileset_builder', tileset_builder)
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
            tilemap_editor = TileMap2DEditor(self, self.project.get_directory())
            self._register_panel('tilemap_editor', tilemap_editor)
            self.addDockWidget(Qt.DockWidgetArea.RightDockWidgetArea, tilemap_editor)
            tilemap_editor.setVisible(True)
            tilemap_editor.raise_()
            tilemap_editor.setFocus()

    def _show_tilemap_25d_editor(self):
        """Show and focus the TileMap 2.5D Editor tool"""
        if 'tilemap_25d_editor' in self.panels:
            tilemap_25d_editor = self.panels['tilemap_25d_editor']
            if not tilemap_25d_editor.isVisible():
                self.addDockWidget(Qt.DockWidgetArea.RightDockWidgetArea, tilemap_25d_editor)
            tilemap_25d_editor.setVisible(True)
            tilemap_25d_editor.raise_()
            tilemap_25d_editor.setFocus()
        else:
            tilemap_25d_editor = TileMap25DEditor(self.editor_bridge, self, self.editor_session, self.project.get_directory())
            self._register_panel('tilemap_25d_editor', tilemap_25d_editor)
            self.addDockWidget(Qt.DockWidgetArea.RightDockWidgetArea, tilemap_25d_editor)
            tilemap_25d_editor.setVisible(True)
            tilemap_25d_editor.raise_()
            tilemap_25d_editor.setFocus()

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
            scribbler = ScribblerTool(self, self.project.get_directory())
            self._register_panel('scribbler', scribbler)
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
            pixel_painter = PixelPainterTool(self, self.project.get_directory())
            self._register_panel('PixelPainterTool', pixel_painter)
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
            voxel_builder = VoxelBuilderTool(self.editor_bridge, self, self.editor_session)
            self._register_panel('VoxelBuilderTool', voxel_builder)
            self.addDockWidget(Qt.DockWidgetArea.RightDockWidgetArea, voxel_builder)
            voxel_builder.setVisible(True)
            voxel_builder.raise_()
            voxel_builder.setFocus()

    def _new_scene(self):
        """Create a new scene"""
        # Preserve the outgoing scene's edits before switching active scene
        self._sync_scene_dirty()
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

    def _sync_scene_dirty(self):
        """Fold the editor bridge's live edit flag into the active scene document.

        The bridge raises a single dirty flag for every kind of edit (gizmo
        drags, property/tree changes via the C++ command system, etc.), but it
        resets on scene switch and is never cleared by a document-level save.
        Consume it into the per-scene document that save/close logic reads, then
        clear it so a save cannot leave the flag stale. Call this before any
        active-scene switch and before any point that reads the dirty state.
        """
        if not self.editor_bridge:
            return
        if not self.editor_bridge.is_active_scene_dirty():
            return
        scene_doc = self.editor_session.get_active_scene_document()
        if scene_doc:
            scene_doc.set_dirty(True)
        if hasattr(self.editor_bridge, 'set_scene_dirty'):
            self.editor_bridge.set_scene_dirty(False)

    def _clear_bridge_dirty(self):
        """Clear the editor bridge dirty flag after the active scene was saved"""
        if self.editor_bridge and hasattr(self.editor_bridge, 'set_scene_dirty'):
            self.editor_bridge.set_scene_dirty(False)

    def _focused_editor_panel(self):
        """Return the EditorPanel that currently contains keyboard focus, if any"""
        widget = QApplication.focusWidget()
        while widget is not None:
            if isinstance(widget, EditorPanel):
                return widget
            widget = widget.parentWidget()
        return None

    def _on_save(self):
        """Context-aware save (Ctrl+S): delegate to the focused panel, else save the scene"""
        panel = self._focused_editor_panel()
        if panel is not None and panel.handle_save():
            return
        self._save_scene()

    def _on_save_as(self):
        """Context-aware save-as (Ctrl+Shift+S): delegate to the focused panel, else save the scene as"""
        panel = self._focused_editor_panel()
        if panel is not None and panel.handle_save_as():
            return
        self._save_scene_as()

    def _on_save_all(self):
        """Save everything (Ctrl+Alt+S): the focused document plus all scenes and scripts"""
        panel = self._focused_editor_panel()
        if panel is not None:
            panel.handle_save()
        self._save_all_scenes()

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
            self._note_scene_self_save(scene_doc.get_file_path())
            self._clear_bridge_dirty()
            scene_name = scene_doc.get_display_name()
            self.status_bar.showMessage(f"Saved scene: {scene_name}", 3000)
            if 'console' in self.panels:
                self.panels['console'].log_message(f"Saved scene: {scene_name}", "Info")
        else:
            QMessageBox.critical(self, "Save Failed", "Failed to save scene.")

    def _rekey_scene_viewport(self, old_key: str, new_path: str):
        """Re-key the scene_viewports mapping after a Save As.

        New scenes are tracked under their display name (e.g. 'Untitled') and
        opened scenes under their file path. After a Save As the document gets a
        real file path, so the mapping key and the viewport tab title must be
        updated to the new path - otherwise Play Scene resolves the stale key and
        refuses to run the 'unsaved' scene.
        """
        viewport = self.scene_viewports.get(old_key)
        if viewport is None:
            return
        if old_key != new_path:
            del self.scene_viewports[old_key]
        self.scene_viewports[new_path] = viewport

        new_name = Path(new_path).stem
        viewport.scene_name = new_name
        for i in range(self.viewport_tabs.count()):
            if self.viewport_tabs.widget(i) == viewport:
                self.viewport_tabs.setTabText(i, new_name)
                break

    def _document_save_params(self, scene_doc):
        """Return (dialog_title, default_dir, default_ext, name_filter) for a Save As

        dialog, picking the prefab format for prefab documents and the scene
        format otherwise so each document type round-trips to the correct file.
        """
        # get_kind()/DocumentKind are added by the prefab-editing C++ build; fall
        # back to the scene format if the runtime predates that rebuild.
        is_prefab = False
        if hasattr(scene_doc, "get_kind") and hasattr(le, "DocumentKind"):
            is_prefab = scene_doc.get_kind() == le.DocumentKind.Prefab
        project_dir = Path(self.project.get_directory())
        if is_prefab:
            default_dir = project_dir / "prefab"
            return ("Save Prefab As", default_dir, ".prefab",
                    "Prefab Files (*.prefab);;All Files (*.*)")
        return ("Save Scene As", project_dir, ".scene",
                "Scene Files (*.scene);;All Files (*.*)")

    def _save_scene_as(self):
        """Save the current active scene with a new file path"""
        # Get active scene document
        scene_doc = self.editor_session.get_active_scene_document()
        if not scene_doc:
            QMessageBox.warning(self, "No Scene", "No active scene document.")
            return

        # Show file dialog to select save location
        title, default_dir, default_ext, name_filter = self._document_save_params(scene_doc)
        file_path, _ = QFileDialog.getSaveFileName(
            self,
            title,
            str(default_dir / f"{scene_doc.get_display_name()}{default_ext}"),
            name_filter
        )

        if file_path:
            # Ensure the document's native extension
            if not file_path.endswith(default_ext):
                file_path += default_ext

            # Capture the pre-save mapping key before save_scene_as changes the path
            old_file = scene_doc.get_file_path()
            old_key = old_file if old_file else scene_doc.get_display_name()

            # Save scene as
            if self.editor_session.save_scene_as(scene_doc.get_id(), file_path):
                self._note_scene_self_save(file_path)
                self._rekey_scene_viewport(old_key, file_path)
                self._clear_bridge_dirty()
                scene_name = scene_doc.get_display_name()
                self.status_bar.showMessage(f"Saved as: {file_path}", 3000)
                if 'console' in self.panels:
                    self.panels['console'].log_message(f"Saved as: {file_path}", "Info")
            else:
                QMessageBox.critical(self, "Save Failed", "Failed to save.")

    def _save_scene_as_for_document(self, scene_doc):
        """Save a specific scene document with a new file path"""
        # Show file dialog to select save location
        title, default_dir, default_ext, name_filter = self._document_save_params(scene_doc)
        file_path, _ = QFileDialog.getSaveFileName(
            self,
            title,
            str(default_dir / f"{scene_doc.get_display_name()}{default_ext}"),
            name_filter
        )

        if file_path:
            # Ensure the document's native extension
            if not file_path.endswith(default_ext):
                file_path += default_ext

            # Capture the pre-save mapping key before save_scene_as changes the path
            old_file = scene_doc.get_file_path()
            old_key = old_file if old_file else scene_doc.get_display_name()

            # Save scene as
            if self.editor_session.save_scene_as(scene_doc.get_id(), file_path):
                self._note_scene_self_save(file_path)
                self._rekey_scene_viewport(old_key, file_path)
                scene_name = scene_doc.get_display_name()
                self.status_bar.showMessage(f"Saved as: {file_path}", 3000)
                if 'console' in self.panels:
                    self.panels['console'].log_message(f"Saved as: {file_path}", "Info")
                return True
            else:
                QMessageBox.critical(self, "Save Failed", "Failed to save.")
                return False

        return False  # User cancelled

    def _save_all_scenes(self):
        """Save all open scenes and scripts

        Saves every open scene that has a file path unconditionally via the
        same path used by Ctrl+S. The native save_all_scenes() only writes
        scenes flagged dirty, but editor edits do not mark documents dirty, so
        relying on it (as save-on-play did) silently skips everything.
        """
        all_success = True

        for path in list(self.scene_viewports.keys()):
            scene_doc = self.editor_session.get_scene_document_by_path(path)
            if not scene_doc or not scene_doc.get_file_path():
                continue
            if not self.editor_session.save_scene(scene_doc.get_id()):
                all_success = False
            else:
                self._note_scene_self_save(scene_doc.get_file_path())

        # Every open scene was just written, so clear the live edit flag
        self._clear_bridge_dirty()

        # Also save all scripts
        if 'script_editor' in self.panels:
            self.panels['script_editor']._save_all_scripts()

        if all_success:
            self.status_bar.showMessage("All scenes and scripts saved", 3000)
            if 'console' in self.panels:
                self.panels['console'].log_message("All scenes and scripts saved", "Info")
        else:
            QMessageBox.warning(self, "Save Failed", "Failed to save one or more scenes.")

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

    def _setup_asset_file_watcher(self):
        """Setup asset file watcher for automatic asset importing with .meta files"""
        project_dir = str(self.project.get_directory())

        # Create asset file watcher with popup enabled
        self.asset_file_watcher = AssetFileWatcher(project_dir, parent=self, show_popup=True)

        # Set up import callback to use AssetDatabase
        def import_asset_callback(path: str) -> bool:
            try:
                asset_db = le.AssetDatabase.get_instance()
                if asset_db.is_initialized():
                    meta = asset_db.import_asset(path)
                    return meta is not None and meta.uuid.is_valid()
                return False
            except Exception as e:
                print(f"[AssetFileWatcher] Import error: {e}")
                return False

        def delete_asset_callback(path: str) -> bool:
            try:
                asset_db = le.AssetDatabase.get_instance()
                if asset_db.is_initialized():
                    # Convert to res:// path for removal
                    res_path = asset_db.to_resource_path(path)
                    if res_path:
                        return asset_db.remove_asset(res_path)
                return False
            except Exception as e:
                print(f"[AssetFileWatcher] Delete error: {e}")
                return False

        self.asset_file_watcher.set_import_callback(import_asset_callback)
        self.asset_file_watcher.set_delete_callback(delete_asset_callback)

        # Connect signals for logging
        self.asset_file_watcher.asset_imported.connect(self._on_asset_imported)
        self.asset_file_watcher.asset_updated.connect(self._on_asset_updated)
        self.asset_file_watcher.asset_deleted.connect(self._on_asset_deleted)
        self.asset_file_watcher.scan_complete.connect(self._on_asset_scan_complete)

        # Start watching
        self.asset_file_watcher.start()

    def _maybe_rescan_archetypes(self, path: str):
        """Rescan archetype definitions and refresh the inspector when a definition changes.

        Only definition files (.archetype, and scripts that may carry @archetype_class) trigger
        this. Instance (.ares) writes are skipped here, both because they do not change schemas and
        to avoid a feedback loop when the inspector saves the instance the user is editing.
        """
        ext = Path(path).suffix.lower()
        if ext not in ('.archetype', '.lua', '.py', '.rb'):
            return

        try:
            self.editor_bridge.rescan_archetypes()
        except Exception:
            pass

        inspector = self.panels.get('inspector')
        if inspector and getattr(inspector, 'current_archetype_path', None):
            inspector._refresh_archetype()

    def _on_asset_imported(self, path: str):
        """Handle asset imported event"""
        if 'console' in self.panels:
            self.panels['console'].log_message(f"Asset imported: {Path(path).name}", "Info")
        self._maybe_rescan_archetypes(path)

    def _on_asset_updated(self, path: str):
        """Handle asset updated event"""
        self._maybe_rescan_archetypes(path)
        ext = Path(path).suffix.lower()

        # A scene file changed on disk. If it is open in the editor, reload it
        # from disk (editor-side only; running play instances are untouched). Our
        # own saves are ignored to avoid a save -> reload feedback loop.
        if ext == '.scene':
            self._reload_scene_from_disk(path)
            return

        # An archetype instance changed on disk (edited externally, or written by a
        # running play instance). Refresh the inspector if it is showing that .ares;
        # the inspector ignores its own recent saves to avoid a feedback loop.
        if ext == '.ares':
            inspector = self.panels.get('inspector')
            if inspector and hasattr(inspector, 'refresh_archetype_if_external'):
                inspector.refresh_archetype_if_external(path)

        # Check if this is a script file - reload any script components using it
        if ext in ('.py', '.lua', '.rb'):
            self._on_script_saved(path)
        else:
            # For non-script assets (textures, models, fonts, audio), use the asset reload system
            try:
                # Convert to res:// path if possible
                if self.project and self.project.path:
                    try:
                        rel_path = Path(path).relative_to(self.project.path)
                        res_path = f"res://{rel_path.as_posix()}"
                    except ValueError:
                        res_path = path
                else:
                    res_path = path

                # Call the C++ asset reload system
                affected = self.editor_bridge.reload_asset(res_path)

                # Auto hot-reload: push the asset change into any running play
                # instances so textures/models/fonts/audio refresh live.
                if self.runtime.is_running():
                    self.runtime.push_asset_reload(res_path)

                if affected > 0:
                    print(f"[AssetReload] Reloaded '{Path(path).name}', affected {affected} components")
                    if 'console' in self.panels:
                        self.panels['console'].log_message(
                            f"Asset reloaded: {Path(path).name} ({affected} components refreshed)", "Info")
                else:
                    print(f"[AssetReload] Asset changed but no components using it: {Path(path).name}")
            except Exception as e:
                print(f"[AssetReload] Error reloading asset: {e}")

    @staticmethod
    def _normalize_fs_path(path: str) -> str:
        """Normalize a filesystem path for cross-source comparison."""
        if not path:
            return ""
        import os
        try:
            return os.path.normcase(os.path.abspath(str(path)))
        except Exception:
            return os.path.normcase(str(path))

    def _note_scene_self_save(self, path: str):
        """Record that the editor itself just wrote this scene file, so the file
        watcher's echo of that write does not trigger an external-edit reload."""
        if not path:
            return
        import time
        self._scene_self_saves[self._normalize_fs_path(path)] = time.time()

    def _reload_scene_from_disk(self, scene_path: str):
        """Reload an open scene from disk after it changed externally.

        Editor-side only: the open scene document is replaced with the on-disk
        version and rebound into its existing viewport tab. Saves the editor just
        made are ignored, and a scene with unsaved in-editor edits prompts before
        its changes are discarded."""
        import time

        # Match the changed file against an open scene viewport (paths may differ
        # in case / separators between the watcher and the stored mapping key).
        normalized = self._normalize_fs_path(scene_path)
        matched_path = None
        for open_path in self.scene_viewports.keys():
            if self._normalize_fs_path(open_path) == normalized:
                matched_path = open_path
                break
        if matched_path is None:
            return  # Scene is not open in the editor; nothing to reload.

        # Ignore the watcher echo of a save the editor just performed.
        last_self_save = self._scene_self_saves.get(normalized, 0.0)
        if time.time() - last_self_save < 2.0:
            return

        viewport = self.scene_viewports.get(matched_path)
        if viewport is None:
            return

        scene_doc = self.editor_session.get_scene_document_by_path(matched_path)

        # Capture any pending in-memory edits into the document so its dirty flag
        # is accurate, then guard against silently discarding unsaved work.
        self._sync_scene_dirty()
        if scene_doc and scene_doc.is_dirty():
            answer = QMessageBox.question(
                self, "Scene Changed on Disk",
                f"'{Path(matched_path).name}' was modified outside the editor, "
                f"but you have unsaved changes.\n\nReload from disk and discard "
                f"your changes?",
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
                QMessageBox.StandardButton.No)
            if answer != QMessageBox.StandardButton.Yes:
                return

        was_active = (self.viewport_tabs.get_current_viewport() is viewport)

        # Drop the stale in-memory document, then reopen from disk. Closing first
        # ensures open_scene re-reads the file rather than returning the cached doc.
        if scene_doc:
            self.editor_session.close_scene(scene_doc.get_id(), False)

        new_doc = self.editor_session.open_scene(matched_path)
        if not new_doc:
            if 'console' in self.panels:
                self.panels['console'].log_message(
                    f"Failed to reload scene from disk: {Path(matched_path).name}", "Error")
            return

        new_scene = new_doc.get_scene()
        viewport.set_scene(new_scene)

        # The inspector / scene tree may reference nodes from the old scene.
        if was_active:
            self.editor_session.set_active_scene(new_doc.get_id())
            self.editor_bridge.set_active_scene(new_scene)
            if 'scene_tree' in self.panels:
                self.panels['scene_tree'].set_scene(new_scene)
            if 'inspector' in self.panels:
                self.panels['inspector'].set_node(None)

        if 'console' in self.panels:
            self.panels['console'].log_message(
                f"Scene reloaded from disk: {Path(matched_path).name}", "Info")

    def _on_asset_deleted(self, path: str):
        """Handle asset deleted event"""
        if 'console' in self.panels:
            self.panels['console'].log_message(f"Asset removed: {Path(path).name}", "Info")
        self._maybe_rescan_archetypes(path)

    def _on_asset_scan_complete(self):
        """Handle asset scan complete event"""
        if 'console' in self.panels:
            self.panels['console'].log_message("Asset database scan complete", "Info")

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

    def _flush_animation_editors(self):
        """Write any pending animation clip edits to disk before running the game.

        The runtime loads .animclip FILES, but the timeline edits an in-memory model;
        flushing here guarantees the authored keyframes are on disk when the game starts.
        """
        panel = self.panels.get('animation_timeline')
        if panel is not None and hasattr(panel, 'flush_pending'):
            panel.flush_pending()

    def _on_play_game(self):
        """Handle Play Game button click"""
        self._flush_animation_editors()
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

        # Play the game using the same graphics backend as the editor
        instance_count = max(1, int(getattr(self.project, 'runtime_instance_count', 1)))
        if self.runtime.play_game(
            project_path=project_path,
            window_width=self.project.window_width,
            window_height=self.project.window_height,
            debugging=True,
            blocking=False,
            backend=self.graphics_backend,
            instance_count=instance_count,
            runtime_args=getattr(self.project, 'runtime_args', ''),
            unique_user_dir=bool(getattr(self.project, 'runtime_unique_user_dir', True))
        ):
            if instance_count > 1:
                self.status_bar.showMessage(f"Launched {instance_count} instances", 3000)
            else:
                self.status_bar.showMessage("Game started", 3000)
            if 'console' in self.panels:
                detail = f" ({instance_count} instances)" if instance_count > 1 else ""
                self.panels['console'].log_message(f"Playing game: {self.project.name}{detail}", "Info")
        else:
            QMessageBox.critical(self, "Runtime Error", "Failed to start game runtime.")

    def _on_play_scene(self):
        """Handle Play Scene button click"""
        self._flush_animation_editors()
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

        # Play the scene using the same graphics backend as the editor
        instance_count = max(1, int(getattr(self.project, 'runtime_instance_count', 1)))
        if self.runtime.play_scene(
            project_path=project_path,
            scene_path=scene_path,
            window_width=self.project.window_width,
            window_height=self.project.window_height,
            debugging=True,
            blocking=False,
            backend=self.graphics_backend,
            instance_count=instance_count,
            runtime_args=getattr(self.project, 'runtime_args', ''),
            unique_user_dir=bool(getattr(self.project, 'runtime_unique_user_dir', True))
        ):
            scene_name = Path(scene_path).stem
            if instance_count > 1:
                self.status_bar.showMessage(f"Playing scene: {scene_name} ({instance_count} instances)", 3000)
            else:
                self.status_bar.showMessage(f"Playing scene: {scene_name}", 3000)
            if 'console' in self.panels:
                detail = f" ({instance_count} instances)" if instance_count > 1 else ""
                self.panels['console'].log_message(f"Playing scene: {scene_path}{detail}", "Info")
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

    def _on_step(self):
        """Handle Step button click (advance one frame while paused)"""
        if not self.runtime.is_running():
            return
        self.runtime.step()
        # Stepping implies paused; reflect that on the Pause toggle.
        self.pause_btn.setText("▶ Resume")
        self.status_bar.showMessage("Stepped one frame", 2000)

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

        # Step - enabled when running and paused (single-frame advance)
        self.step_btn.setEnabled(is_running and is_paused)

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
        elif self._focus_in_panel('animation_timeline'):
            self.panels['animation_timeline'].copy_keys()
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
        elif self._focus_in_panel('animation_timeline'):
            self.panels['animation_timeline'].paste_keys()
        elif 'scene_tree' in self.panels:
            # Paste nodes to scene tree
            self.panels['scene_tree'].paste_nodes()

    def _focus_in_panel(self, panel_id: str) -> bool:
        """True when keyboard focus is inside the given (visible) panel."""
        panel = self.panels.get(panel_id)
        if not panel or not panel.isVisible():
            return False
        widget = self.focusWidget()
        while widget is not None:
            if widget is panel:
                return True
            widget = widget.parentWidget()
        return False

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
        elif self._focus_in_panel('animation_timeline'):
            self.panels['animation_timeline']._on_delete_key()
        elif 'scene_tree' in self.panels:
            # Delete nodes from scene tree
            self.panels['scene_tree']._on_delete_node_clicked()

    def _on_duplicate(self):
        """Duplicate selected node(s)"""
        if 'scene_tree' in self.panels:
            self.panels['scene_tree']._on_duplicate_node_clicked()

    def changeEvent(self, event):
        """Handle window state changes - check for file changes when window gains focus"""
        from PyQt6.QtCore import QEvent
        if event.type() == QEvent.Type.ActivationChange:
            if self.isActiveWindow():
                # Window just gained focus - check for external file changes
                if self.asset_file_watcher:
                    self.asset_file_watcher.check_for_changes()
        super().changeEvent(event)

    def closeEvent(self, event):
        """Handle window close event"""
        # Save layout before showing dialog
        self._save_layout()

        # Confirm close
        reply = QMessageBox.question(
            self,
            "Close Editor",
            "Are you sure you want to close the editor?",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
        )

        if reply == QMessageBox.StandardButton.Yes:
            # Stop all timers first to prevent callbacks during cleanup
            if hasattr(self, 'runtime_state_timer') and self.runtime_state_timer:
                self.runtime_state_timer.stop()

            # Cleanup runtime (stops and releases all resources)
            if hasattr(self, 'runtime') and self.runtime:
                self.runtime.cleanup()

            # Disable plugins so they release docks/menus/resources cleanly.
            # The persisted enabled set is preserved for the next session.
            if hasattr(self, 'plugin_manager') and self.plugin_manager:
                try:
                    self.plugin_manager.shutdown()
                except Exception as e:
                    print(f"Error shutting down plugins: {e}")

            # Stop asset file watcher
            if self.asset_file_watcher:
                self.asset_file_watcher.stop()

            # Cleanup all viewports
            self.viewport_tabs.cleanup_all()

            # Cleanup tool panels (stop their timers and release resources)
            self._cleanup_panels()

            # Shutdown editor bridge and session
            self.editor_bridge.shutdown()
            self.editor_session.shutdown()

            event.accept()
        else:
            event.ignore()

    def _cleanup_panels(self):
        """Cleanup all panels and their resources"""
        for name, panel in list(self.panels.items()):
            # Stop any timers the panel might have
            if hasattr(panel, 'timer') and panel.timer:
                panel.timer.stop()
            if hasattr(panel, '_timer') and panel._timer:
                panel._timer.stop()
            if hasattr(panel, 'update_timer') and panel.update_timer:
                panel.update_timer.stop()
            if hasattr(panel, 'debounce_timer') and panel.debounce_timer:
                panel.debounce_timer.stop()
            # Call cleanup method if panel has one
            if hasattr(panel, 'cleanup') and callable(panel.cleanup):
                try:
                    panel.cleanup()
                except Exception as e:
                    print(f"Error cleaning up panel {name}: {e}")
            # Call close method if panel has one (for tool windows)
            if hasattr(panel, 'close') and callable(panel.close):
                try:
                    panel.close()
                except Exception:
                    pass
        self.panels.clear()
