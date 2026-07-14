"""
Project Settings Dialog
Godot-style project settings editor with multiple tabs
"""

from PyQt6.QtWidgets import (QDialog, QVBoxLayout, QHBoxLayout, QTabWidget,
                             QPushButton, QWidget, QLabel, QLineEdit, QSpinBox,
                             QCheckBox, QComboBox, QGroupBox, QFormLayout,
                             QMessageBox, QScrollArea, QDoubleSpinBox, QFileDialog,
                             QListWidget, QInputDialog, QColorDialog)
from PyQt6.QtGui import QColor
from PyQt6.QtCore import Qt, pyqtSignal
from pathlib import Path
import sys
import json
import copy

# Add parent directory to path to import project_file
sys.path.insert(0, str(Path(__file__).parent.parent))
from project_file import ProjectFile


class ProjectSettingsDialog(QDialog):
    """Dialog for editing project settings"""
    
    settings_saved = pyqtSignal(object)  # Emits updated project data
    
    def __init__(self, project_data, parent=None):
        super().__init__(parent)
        self.original_project = project_data
        # Work with a copy so we don't modify the original until save
        self.project = copy.deepcopy(project_data)
        self.has_unsaved_changes = False
        
        self.setWindowTitle("Project Settings")
        self.setModal(True)
        self.setMinimumSize(800, 600)

        self._setup_ui()
        self._load_settings()

        # Reset the unsaved changes flag after loading settings
        # (loading settings triggers change signals which would mark as unsaved)
        self.has_unsaved_changes = False
    
    def _setup_ui(self):
        """Setup the dialog UI"""
        layout = QVBoxLayout()
        layout.setSpacing(10)
        
        # Title
        title_label = QLabel("Project Settings")
        title_label.setStyleSheet("font-size: 18px; font-weight: bold;")
        layout.addWidget(title_label)
        
        # Tab widget for different categories
        self.tabs = QTabWidget()
        self.tabs.currentChanged.connect(self._on_tab_changed)
        
        # Create tabs
        self.tabs.addTab(self._create_general_tab(), "General")
        self.tabs.addTab(self._create_window_tab(), "Window")
        self.tabs.addTab(self._create_graphics_tab(), "Graphics")
        self.tabs.addTab(self._create_audio_tab(), "Audio")
        self.tabs.addTab(self._create_physics_tab(), "Physics")
        self.tabs.addTab(self._create_input_tab(), "Input")
        self.tabs.addTab(self._create_networking_tab(), "Networking")
        self.tabs.addTab(self._create_localization_tab(), "Localization")
        self.tabs.addTab(self._create_splash_tab(), "Splash Screens")
        self.tabs.addTab(self._create_editor_tab(), "Editor")
        
        layout.addWidget(self.tabs)
        
        # Buttons
        button_layout = QHBoxLayout()
        button_layout.addStretch()
        
        close_btn = QPushButton("Close")
        close_btn.setProperty("secondary", True)
        close_btn.clicked.connect(self._on_close)
        button_layout.addWidget(close_btn)
        
        save_btn = QPushButton("Save")
        save_btn.clicked.connect(self._on_save)
        button_layout.addWidget(save_btn)
        
        save_close_btn = QPushButton("Save && Close")
        save_close_btn.setProperty("success", True)
        save_close_btn.clicked.connect(self._on_save_and_close)
        button_layout.addWidget(save_close_btn)
        
        layout.addLayout(button_layout)
        
        self.setLayout(layout)
    
    def _create_general_tab(self) -> QWidget:
        """Create the General settings tab"""
        widget = QWidget()
        layout = QVBoxLayout()
        
        # Project Info Group
        info_group = QGroupBox("Project Information")
        info_layout = QFormLayout()
        
        self.project_name_edit = QLineEdit()
        self.project_name_edit.textChanged.connect(self._mark_unsaved)
        info_layout.addRow("Project Name:", self.project_name_edit)
        
        self.creator_edit = QLineEdit()
        self.creator_edit.textChanged.connect(self._mark_unsaved)
        info_layout.addRow("Creator:", self.creator_edit)
        
        self.version_edit = QLineEdit()
        self.version_edit.textChanged.connect(self._mark_unsaved)
        info_layout.addRow("Version:", self.version_edit)
        
        # Icon path with browse button
        icon_layout = QHBoxLayout()
        self.icon_edit = QLineEdit()
        self.icon_edit.setPlaceholderText("Path to project icon (relative to project folder)")
        self.icon_edit.textChanged.connect(self._mark_unsaved)
        icon_layout.addWidget(self.icon_edit)
        
        icon_browse_btn = QPushButton("Browse...")
        icon_browse_btn.clicked.connect(self._browse_icon)
        icon_browse_btn.setFixedWidth(80)
        icon_layout.addWidget(icon_browse_btn)
        
        info_layout.addRow("Icon Path:", icon_layout)
        
        info_group.setLayout(info_layout)
        layout.addWidget(info_group)
        
        # Main Scene Group
        scene_group = QGroupBox("Main Scene")
        scene_layout = QFormLayout()

        self.main_scene_edit = QLineEdit()
        self.main_scene_edit.setPlaceholderText("scenes/main.scene")
        self.main_scene_edit.textChanged.connect(self._mark_unsaved)
        scene_layout.addRow("Main Scene:", self.main_scene_edit)

        scene_group.setLayout(scene_layout)
        layout.addWidget(scene_group)

        # Default Font Group
        font_group = QGroupBox("Default Font")
        font_layout = QFormLayout()

        font_path_layout = QHBoxLayout()
        self.default_font_edit = QLineEdit()
        self.default_font_edit.setPlaceholderText("res://assets/fonts/DefaultFont.ttf (leave empty for engine default)")
        self.default_font_edit.textChanged.connect(self._mark_unsaved)
        font_path_layout.addWidget(self.default_font_edit)

        font_browse_btn = QPushButton("Browse...")
        font_browse_btn.clicked.connect(self._browse_default_font)
        font_browse_btn.setFixedWidth(80)
        font_path_layout.addWidget(font_browse_btn)

        font_layout.addRow("Font Path:", font_path_layout)

        font_info_label = QLabel(
            "The default font is used when a Label component has no font set.\n"
            "Leave empty to use the engine's built-in default font."
        )
        font_info_label.setProperty("secondary", True)
        font_info_label.setWordWrap(True)
        font_info_label.setStyleSheet("padding: 5px; font-style: italic;")
        font_layout.addRow(font_info_label)

        font_group.setLayout(font_layout)
        layout.addWidget(font_group)

        # Default UI Theme Group
        theme_group = QGroupBox("Default UI Theme")
        theme_layout = QFormLayout()

        theme_path_layout = QHBoxLayout()
        self.default_theme_edit = QLineEdit()
        self.default_theme_edit.setPlaceholderText("res://default.uitheme (leave empty for the default.uitheme convention)")
        self.default_theme_edit.textChanged.connect(self._mark_unsaved)
        theme_path_layout.addWidget(self.default_theme_edit)

        theme_browse_btn = QPushButton("Browse...")
        theme_browse_btn.clicked.connect(self._browse_default_theme)
        theme_browse_btn.setFixedWidth(80)
        theme_path_layout.addWidget(theme_browse_btn)

        theme_layout.addRow("Theme Path:", theme_path_layout)

        theme_info_label = QLabel(
            "The UI theme styles in-game UI controls (Button/Panel/Label/etc.).\n"
            "Leave empty to use res://default.uitheme if the project has one."
        )
        theme_info_label.setProperty("secondary", True)
        theme_info_label.setWordWrap(True)
        theme_info_label.setStyleSheet("padding: 5px; font-style: italic;")
        theme_layout.addRow(theme_info_label)

        theme_group.setLayout(theme_layout)
        layout.addWidget(theme_group)

        layout.addStretch()
        widget.setLayout(layout)
        return widget
    
    def _create_window_tab(self) -> QWidget:
        """Create the Window settings tab"""
        widget = QWidget()
        layout = QVBoxLayout()
        
        # Design Resolution Group. This is the logical canvas UI is authored in and
        # that anchor layout resolves against - not necessarily the physical window
        # size (see the override group below).
        size_group = QGroupBox("Design Resolution")
        size_layout = QFormLayout()

        size_hint = QLabel(
            "The fixed canvas the game is designed for. UI anchors resolve against "
            "this size; the window is scaled to it via the Graphics scale mode."
        )
        size_hint.setWordWrap(True)
        size_layout.addRow(size_hint)

        self.window_width_spin = QSpinBox()
        self.window_width_spin.setRange(640, 7680)
        self.window_width_spin.setSingleStep(10)
        self.window_width_spin.valueChanged.connect(self._mark_unsaved)
        size_layout.addRow("Width:", self.window_width_spin)

        self.window_height_spin = QSpinBox()
        self.window_height_spin.setRange(480, 4320)
        self.window_height_spin.setSingleStep(10)
        self.window_height_spin.valueChanged.connect(self._mark_unsaved)
        size_layout.addRow("Height:", self.window_height_spin)

        size_group.setLayout(size_layout)
        layout.addWidget(size_group)

        # Window Size Override Group. Opens the OS window at a different physical
        # size than the design resolution; the design canvas is letterboxed/scaled
        # onto it. Off => the window opens at the design resolution.
        override_group = QGroupBox("Window Size Override")
        override_layout = QFormLayout()

        self.window_size_override_check = QCheckBox(
            "Open window at a different size than the design resolution"
        )
        self.window_size_override_check.stateChanged.connect(self._mark_unsaved)
        self.window_size_override_check.stateChanged.connect(
            self._on_window_size_override_toggled
        )
        override_layout.addRow(self.window_size_override_check)

        self.window_size_override_width_spin = QSpinBox()
        self.window_size_override_width_spin.setRange(640, 7680)
        self.window_size_override_width_spin.setSingleStep(10)
        self.window_size_override_width_spin.valueChanged.connect(self._mark_unsaved)
        override_layout.addRow("Window Width:", self.window_size_override_width_spin)

        self.window_size_override_height_spin = QSpinBox()
        self.window_size_override_height_spin.setRange(480, 4320)
        self.window_size_override_height_spin.setSingleStep(10)
        self.window_size_override_height_spin.valueChanged.connect(self._mark_unsaved)
        override_layout.addRow("Window Height:", self.window_size_override_height_spin)

        override_group.setLayout(override_layout)
        layout.addWidget(override_group)

        # Window Mode Group
        mode_group = QGroupBox("Window Mode")
        mode_layout = QFormLayout()
        
        self.fullscreen_check = QCheckBox("Start in Fullscreen")
        self.fullscreen_check.stateChanged.connect(self._mark_unsaved)
        mode_layout.addRow(self.fullscreen_check)
        
        self.resizable_check = QCheckBox("Allow Window Resize")
        self.resizable_check.setChecked(True)
        self.resizable_check.stateChanged.connect(self._mark_unsaved)
        mode_layout.addRow(self.resizable_check)
        
        self.borderless_check = QCheckBox("Borderless Window")
        self.borderless_check.stateChanged.connect(self._mark_unsaved)
        mode_layout.addRow(self.borderless_check)
        
        mode_group.setLayout(mode_layout)
        layout.addWidget(mode_group)
        
        layout.addStretch()
        widget.setLayout(layout)
        return widget

    def _on_window_size_override_toggled(self) -> None:
        """Enable the override size spinboxes only when the override is active."""
        enabled = self.window_size_override_check.isChecked()
        self.window_size_override_width_spin.setEnabled(enabled)
        self.window_size_override_height_spin.setEnabled(enabled)

    def _create_graphics_tab(self) -> QWidget:
        """Create the Graphics settings tab"""
        widget = QWidget()
        layout = QVBoxLayout()
        
        # Rendering Group
        render_group = QGroupBox("Rendering")
        render_layout = QFormLayout()
        
        self.vsync_check = QCheckBox("Enable VSync")
        self.vsync_check.setChecked(True)
        self.vsync_check.stateChanged.connect(self._mark_unsaved)
        render_layout.addRow(self.vsync_check)
        
        self.target_fps_spin = QSpinBox()
        self.target_fps_spin.setRange(30, 240)
        self.target_fps_spin.setSingleStep(10)
        self.target_fps_spin.setValue(60)
        self.target_fps_spin.valueChanged.connect(self._mark_unsaved)
        render_layout.addRow("Target FPS:", self.target_fps_spin)
        
        self.backend_combo = QComboBox()
        self.backend_combo.addItems(["OpenGL", "Vulkan", "DirectX 11", "DirectX 12"])
        self.backend_combo.setEnabled(False)
        self.backend_combo.setToolTip(
            "The graphics backend is selected per export preset (Export dialog) "
            "or via the runtime's --renderer flag, not in project settings."
        )
        render_layout.addRow("Graphics Backend:", self.backend_combo)
        
        render_group.setLayout(render_layout)
        layout.addWidget(render_group)
        
        # Display Settings Group
        display_group = QGroupBox("Display Settings")
        display_layout = QFormLayout()

        self.scale_mode_combo = QComboBox()
        self.scale_mode_combo.addItems(["Letterbox", "Stretch", "Crop", "Ignore"])
        self.scale_mode_combo.currentIndexChanged.connect(self._mark_unsaved)
        display_layout.addRow("Scale Mode:", self.scale_mode_combo)

        self.texture_filtering_combo = QComboBox()
        self.texture_filtering_combo.addItems(["Cubic", "Bilinear", "Nearest Neighbor"])
        self.texture_filtering_combo.setCurrentIndex(1)  # Default to Bilinear
        self.texture_filtering_combo.currentIndexChanged.connect(self._mark_unsaved)
        display_layout.addRow("Texture Filtering:", self.texture_filtering_combo)

        # Background clear color (RGBA) used by the runtime cameras
        clear_color_layout = QHBoxLayout()
        self.clear_color_button = QPushButton()
        self.clear_color_button.setFixedSize(48, 24)
        self.clear_color_button.clicked.connect(self._pick_clear_color)
        clear_color_layout.addWidget(self.clear_color_button)
        clear_color_layout.addStretch()
        self._clear_color = [0.1, 0.1, 0.1, 1.0]
        self._update_clear_color_swatch()
        display_layout.addRow("Clear Color:", clear_color_layout)

        display_group.setLayout(display_layout)
        layout.addWidget(display_group)

        # Quality Group (not yet honored by the runtime renderer)
        quality_group = QGroupBox("Quality Settings (not yet supported)")
        quality_layout = QFormLayout()

        _quality_tip = "Not yet honored by the runtime renderer."

        self.msaa_combo = QComboBox()
        self.msaa_combo.addItems(["Disabled", "2x", "4x", "8x", "16x"])
        self.msaa_combo.setEnabled(False)
        self.msaa_combo.setToolTip(_quality_tip)
        quality_layout.addRow("MSAA:", self.msaa_combo)

        self.shadow_quality_combo = QComboBox()
        self.shadow_quality_combo.addItems(["Low", "Medium", "High", "Ultra"])
        self.shadow_quality_combo.setCurrentIndex(2)
        self.shadow_quality_combo.setEnabled(False)
        self.shadow_quality_combo.setToolTip(_quality_tip)
        quality_layout.addRow("Shadow Quality:", self.shadow_quality_combo)

        self.texture_quality_combo = QComboBox()
        self.texture_quality_combo.addItems(["Low", "Medium", "High", "Ultra"])
        self.texture_quality_combo.setCurrentIndex(3)
        self.texture_quality_combo.setEnabled(False)
        self.texture_quality_combo.setToolTip(_quality_tip)
        quality_layout.addRow("Texture Quality:", self.texture_quality_combo)

        quality_group.setLayout(quality_layout)
        layout.addWidget(quality_group)
        
        layout.addStretch()
        widget.setLayout(layout)
        return widget
    
    def _create_audio_tab(self) -> QWidget:
        """Create the Audio settings tab"""
        widget = QWidget()
        layout = QVBoxLayout()
        
        # Master Volume Group
        volume_group = QGroupBox("Volume Settings")
        volume_layout = QFormLayout()
        
        self.master_volume_spin = QDoubleSpinBox()
        self.master_volume_spin.setRange(0.0, 1.0)
        self.master_volume_spin.setSingleStep(0.1)
        self.master_volume_spin.setValue(1.0)
        self.master_volume_spin.valueChanged.connect(self._mark_unsaved)
        volume_layout.addRow("Master Volume:", self.master_volume_spin)
        
        self.music_volume_spin = QDoubleSpinBox()
        self.music_volume_spin.setRange(0.0, 1.0)
        self.music_volume_spin.setSingleStep(0.1)
        self.music_volume_spin.setValue(1.0)
        self.music_volume_spin.valueChanged.connect(self._mark_unsaved)
        volume_layout.addRow("Music Volume:", self.music_volume_spin)
        
        self.sfx_volume_spin = QDoubleSpinBox()
        self.sfx_volume_spin.setRange(0.0, 1.0)
        self.sfx_volume_spin.setSingleStep(0.1)
        self.sfx_volume_spin.setValue(1.0)
        self.sfx_volume_spin.valueChanged.connect(self._mark_unsaved)
        volume_layout.addRow("SFX Volume:", self.sfx_volume_spin)
        
        volume_group.setLayout(volume_layout)
        layout.addWidget(volume_group)
        
        # Audio Settings Group (backend/sample rate are auto-selected by miniaudio)
        audio_group = QGroupBox("Audio Settings (not yet supported)")
        audio_layout = QFormLayout()

        _audio_tip = "The audio backend and sample rate are auto-selected by the engine."

        self.audio_backend_combo = QComboBox()
        self.audio_backend_combo.addItems(["Auto", "OpenAL", "XAudio2", "WASAPI"])
        self.audio_backend_combo.setEnabled(False)
        self.audio_backend_combo.setToolTip(_audio_tip)
        audio_layout.addRow("Audio Backend:", self.audio_backend_combo)

        self.sample_rate_combo = QComboBox()
        self.sample_rate_combo.addItems(["44100 Hz", "48000 Hz", "96000 Hz"])
        self.sample_rate_combo.setCurrentIndex(1)
        self.sample_rate_combo.setEnabled(False)
        self.sample_rate_combo.setToolTip(_audio_tip)
        audio_layout.addRow("Sample Rate:", self.sample_rate_combo)

        audio_group.setLayout(audio_layout)
        layout.addWidget(audio_group)
        
        layout.addStretch()
        widget.setLayout(layout)
        return widget
    
    def _create_physics_tab(self) -> QWidget:
        """Create the Physics settings tab"""
        widget = QWidget()
        layout = QVBoxLayout()

        # 2D Physics Group
        physics_2d_group = QGroupBox("2D Physics")
        physics_2d_layout = QFormLayout()

        self.gravity_2d_x_spin = QDoubleSpinBox()
        self.gravity_2d_x_spin.setRange(-100000.0, 100000.0)
        self.gravity_2d_x_spin.setValue(0.0)
        self.gravity_2d_x_spin.valueChanged.connect(self._mark_unsaved)
        physics_2d_layout.addRow("Gravity X:", self.gravity_2d_x_spin)

        self.gravity_2d_y_spin = QDoubleSpinBox()
        self.gravity_2d_y_spin.setRange(-100000.0, 100000.0)
        self.gravity_2d_y_spin.setValue(-980.0)
        self.gravity_2d_y_spin.valueChanged.connect(self._mark_unsaved)
        physics_2d_layout.addRow("Gravity Y:", self.gravity_2d_y_spin)

        physics_2d_group.setLayout(physics_2d_layout)
        layout.addWidget(physics_2d_group)

        # 3D Physics Group
        physics_3d_group = QGroupBox("3D Physics")
        physics_3d_layout = QFormLayout()

        self.gravity_3d_x_spin = QDoubleSpinBox()
        self.gravity_3d_x_spin.setRange(-1000.0, 1000.0)
        self.gravity_3d_x_spin.setValue(0.0)
        self.gravity_3d_x_spin.valueChanged.connect(self._mark_unsaved)
        physics_3d_layout.addRow("Gravity X:", self.gravity_3d_x_spin)

        self.gravity_3d_y_spin = QDoubleSpinBox()
        self.gravity_3d_y_spin.setRange(-1000.0, 1000.0)
        self.gravity_3d_y_spin.setValue(-9.8)
        self.gravity_3d_y_spin.valueChanged.connect(self._mark_unsaved)
        physics_3d_layout.addRow("Gravity Y:", self.gravity_3d_y_spin)

        self.gravity_3d_z_spin = QDoubleSpinBox()
        self.gravity_3d_z_spin.setRange(-1000.0, 1000.0)
        self.gravity_3d_z_spin.setValue(0.0)
        self.gravity_3d_z_spin.valueChanged.connect(self._mark_unsaved)
        physics_3d_layout.addRow("Gravity Z:", self.gravity_3d_z_spin)

        physics_3d_group.setLayout(physics_3d_layout)
        layout.addWidget(physics_3d_group)

        # General Physics Group
        general_physics_group = QGroupBox("General")
        general_physics_layout = QFormLayout()

        self.physics_fps_spin = QSpinBox()
        self.physics_fps_spin.setRange(30, 240)
        self.physics_fps_spin.setValue(60)
        self.physics_fps_spin.valueChanged.connect(self._mark_unsaved)
        general_physics_layout.addRow("Physics FPS:", self.physics_fps_spin)

        general_physics_group.setLayout(general_physics_layout)
        layout.addWidget(general_physics_group)

        # 2D Collision Layers Group
        layers_2d_group = QGroupBox("2D Collision Layers")
        layers_2d_layout = QVBoxLayout()

        self.layer_2d_edits = []
        layers_2d_scroll = QScrollArea()
        layers_2d_scroll.setWidgetResizable(True)
        layers_2d_content = QWidget()
        layers_2d_form = QFormLayout(layers_2d_content)
        layers_2d_form.setSpacing(4)

        for i in range(32):
            edit = QLineEdit()
            edit.setPlaceholderText(f"Layer {i+1}")
            edit.textChanged.connect(self._mark_unsaved)
            self.layer_2d_edits.append(edit)
            layers_2d_form.addRow(f"{i+1}:", edit)

        layers_2d_scroll.setWidget(layers_2d_content)
        layers_2d_scroll.setMaximumHeight(200)
        layers_2d_layout.addWidget(layers_2d_scroll)
        layers_2d_group.setLayout(layers_2d_layout)
        layout.addWidget(layers_2d_group)

        # 3D Collision Layers Group
        layers_3d_group = QGroupBox("3D Collision Layers")
        layers_3d_layout = QVBoxLayout()

        self.layer_3d_edits = []
        layers_3d_scroll = QScrollArea()
        layers_3d_scroll.setWidgetResizable(True)
        layers_3d_content = QWidget()
        layers_3d_form = QFormLayout(layers_3d_content)
        layers_3d_form.setSpacing(4)

        for i in range(32):
            edit = QLineEdit()
            edit.setPlaceholderText(f"Layer {i+1}")
            edit.textChanged.connect(self._mark_unsaved)
            self.layer_3d_edits.append(edit)
            layers_3d_form.addRow(f"{i+1}:", edit)

        layers_3d_scroll.setWidget(layers_3d_content)
        layers_3d_scroll.setMaximumHeight(200)
        layers_3d_layout.addWidget(layers_3d_scroll)
        layers_3d_group.setLayout(layers_3d_layout)
        layout.addWidget(layers_3d_group)

        layout.addStretch()
        widget.setLayout(layout)
        return widget
    
    def _create_input_tab(self) -> QWidget:
        """Create the Input settings tab"""
        widget = QWidget()
        layout = QVBoxLayout()
        
        # Input Group (these are gameplay conventions, applied in your scripts)
        input_group = QGroupBox("Input Settings (not yet supported)")
        input_layout = QFormLayout()

        _input_tip = ("Mouse sensitivity / invert-Y are gameplay conventions; apply "
                      "them in your input scripts rather than as engine settings.")

        self.mouse_sensitivity_spin = QDoubleSpinBox()
        self.mouse_sensitivity_spin.setRange(0.1, 10.0)
        self.mouse_sensitivity_spin.setSingleStep(0.1)
        self.mouse_sensitivity_spin.setValue(1.0)
        self.mouse_sensitivity_spin.setEnabled(False)
        self.mouse_sensitivity_spin.setToolTip(_input_tip)
        input_layout.addRow("Mouse Sensitivity:", self.mouse_sensitivity_spin)

        self.invert_y_check = QCheckBox("Invert Y Axis")
        self.invert_y_check.setEnabled(False)
        self.invert_y_check.setToolTip(_input_tip)
        input_layout.addRow(self.invert_y_check)
        
        input_group.setLayout(input_layout)
        layout.addWidget(input_group)
        
        # Note about input mapping
        note_label = QLabel("Input mapping can be configured through the Input Map editor\n(Tools > Input Map Editor)")
        note_label.setProperty("secondary", True)
        note_label.setStyleSheet("font-style: italic; padding: 10px;")
        layout.addWidget(note_label)
        
        layout.addStretch()
        widget.setLayout(layout)
        return widget

    def _create_networking_tab(self) -> QWidget:
        """Create the Networking settings tab (defaults read by networked games)."""
        widget = QWidget()
        layout = QVBoxLayout()

        session_group = QGroupBox("Session Defaults")
        session_layout = QFormLayout()

        self.net_transport_combo = QComboBox()
        self.net_transport_combo.addItems(["ENet (native UDP)", "WebSocket", "Loopback (local)"])
        self.net_transport_combo.currentIndexChanged.connect(self._mark_unsaved)
        session_layout.addRow("Default Transport:", self.net_transport_combo)

        self.net_port_spin = QSpinBox()
        self.net_port_spin.setRange(1, 65535)
        self.net_port_spin.setValue(7777)
        self.net_port_spin.valueChanged.connect(self._mark_unsaved)
        session_layout.addRow("Default Port:", self.net_port_spin)

        self.net_max_peers_spin = QSpinBox()
        self.net_max_peers_spin.setRange(1, 4096)
        self.net_max_peers_spin.setValue(32)
        self.net_max_peers_spin.valueChanged.connect(self._mark_unsaved)
        session_layout.addRow("Max Peers:", self.net_max_peers_spin)

        session_group.setLayout(session_layout)
        layout.addWidget(session_group)

        replication_group = QGroupBox("Replication")
        replication_layout = QFormLayout()

        self.net_tick_rate_spin = QDoubleSpinBox()
        self.net_tick_rate_spin.setRange(1.0, 128.0)
        self.net_tick_rate_spin.setValue(30.0)
        self.net_tick_rate_spin.valueChanged.connect(self._mark_unsaved)
        replication_layout.addRow("Snapshot Tick Rate (Hz):", self.net_tick_rate_spin)

        self.net_interp_delay_spin = QDoubleSpinBox()
        self.net_interp_delay_spin.setRange(0.0, 1000.0)
        self.net_interp_delay_spin.setValue(100.0)
        self.net_interp_delay_spin.setToolTip(
            "Receivers render this far behind the newest sample so jitter and "
            "packet-timing variation are absorbed for smooth motion.")
        self.net_interp_delay_spin.valueChanged.connect(self._mark_unsaved)
        replication_layout.addRow("Interpolation Delay (ms):", self.net_interp_delay_spin)

        self.net_keyframe_interval_spin = QDoubleSpinBox()
        self.net_keyframe_interval_spin.setRange(0.0, 10000.0)
        self.net_keyframe_interval_spin.setValue(1000.0)
        self.net_keyframe_interval_spin.setToolTip(
            "Period between reliable full-state keyframes. The per-tick delta "
            "stream is unreliable, so a periodic keyframe re-establishes any value "
            "stranded by a dropped packet. 0 disables keyframes.")
        self.net_keyframe_interval_spin.valueChanged.connect(self._mark_unsaved)
        replication_layout.addRow("Keyframe Interval (ms):", self.net_keyframe_interval_spin)

        self.net_ping_interval_spin = QDoubleSpinBox()
        self.net_ping_interval_spin.setRange(0.0, 30.0)
        self.net_ping_interval_spin.setValue(1.0)
        self.net_ping_interval_spin.setToolTip(
            "How often a round-trip-time probe is sent to each peer. 0 disables it.")
        self.net_ping_interval_spin.valueChanged.connect(self._mark_unsaved)
        replication_layout.addRow("Ping Interval (s):", self.net_ping_interval_spin)

        self.net_interest_radius_spin = QDoubleSpinBox()
        self.net_interest_radius_spin.setRange(0.0, 1000000.0)
        self.net_interest_radius_spin.setValue(0.0)
        self.net_interest_radius_spin.setToolTip(
            "Area-of-interest radius. When > 0 the server replicates each client "
            "only the server-owned objects within this distance of that client's "
            "reported interest position (plus objects marked alwaysRelevant). "
            "0 disables culling (every object goes to every peer).")
        self.net_interest_radius_spin.valueChanged.connect(self._mark_unsaved)
        replication_layout.addRow("Interest Radius:", self.net_interest_radius_spin)

        replication_group.setLayout(replication_layout)
        layout.addWidget(replication_group)

        discovery_group = QGroupBox("LAN Discovery")
        discovery_layout = QFormLayout()

        self.net_lan_discovery_check = QCheckBox("Advertise listening sessions on the LAN")
        self.net_lan_discovery_check.setChecked(False)
        self.net_lan_discovery_check.setToolTip(
            "When enabled, a server/host answers UDP discovery probes so clients on "
            "the same subnet can find it without typing an address.")
        self.net_lan_discovery_check.stateChanged.connect(self._mark_unsaved)
        discovery_layout.addRow("", self.net_lan_discovery_check)

        self.net_discovery_port_spin = QSpinBox()
        self.net_discovery_port_spin.setRange(1, 65535)
        self.net_discovery_port_spin.setValue(7779)
        self.net_discovery_port_spin.valueChanged.connect(self._mark_unsaved)
        discovery_layout.addRow("Discovery Port:", self.net_discovery_port_spin)

        self.net_server_name_edit = QLineEdit()
        self.net_server_name_edit.setText("Lupine Server")
        self.net_server_name_edit.setToolTip("Label shown for this server in a LAN server browser.")
        self.net_server_name_edit.textChanged.connect(self._mark_unsaved)
        discovery_layout.addRow("Server Name:", self.net_server_name_edit)

        discovery_group.setLayout(discovery_layout)
        layout.addWidget(discovery_group)

        compat_group = QGroupBox("Compatibility Gate")
        compat_layout = QFormLayout()

        self.net_protocol_spin = QSpinBox()
        self.net_protocol_spin.setRange(0, 1000000)
        self.net_protocol_spin.setValue(1)
        self.net_protocol_spin.valueChanged.connect(self._mark_unsaved)
        compat_layout.addRow("Protocol Version:", self.net_protocol_spin)

        self.net_game_id_edit = QLineEdit()
        self.net_game_id_edit.setText("lupine-game")
        self.net_game_id_edit.textChanged.connect(self._mark_unsaved)
        compat_layout.addRow("Game Id:", self.net_game_id_edit)

        compat_group.setLayout(compat_layout)
        layout.addWidget(compat_group)

        info = QLabel("Peers whose protocol version or game id differ are rejected, "
                      "so incompatible builds cannot corrupt one another.")
        info.setWordWrap(True)
        layout.addWidget(info)

        reliability_group = QGroupBox("Prediction & Reliability")
        reliability_layout = QFormLayout()

        self.net_prediction_check = QCheckBox("Enable client-side prediction")
        self.net_prediction_check.setChecked(True)
        self.net_prediction_check.setToolTip(
            "Predict locally-controlled (NetworkController) objects and reconcile against the server.")
        self.net_prediction_check.stateChanged.connect(self._mark_unsaved)
        reliability_layout.addRow("", self.net_prediction_check)

        self.net_input_redundancy_spin = QSpinBox()
        self.net_input_redundancy_spin.setRange(1, 32)
        self.net_input_redundancy_spin.setValue(3)
        self.net_input_redundancy_spin.setToolTip("Past input commands repeated in each packet (loss tolerance).")
        self.net_input_redundancy_spin.valueChanged.connect(self._mark_unsaved)
        reliability_layout.addRow("Input Redundancy:", self.net_input_redundancy_spin)

        self.net_auto_reconnect_check = QCheckBox("Auto-reconnect on unexpected drop")
        self.net_auto_reconnect_check.setChecked(False)
        self.net_auto_reconnect_check.stateChanged.connect(self._mark_unsaved)
        reliability_layout.addRow("", self.net_auto_reconnect_check)

        self.net_reconnect_attempts_spin = QSpinBox()
        self.net_reconnect_attempts_spin.setRange(0, 100)
        self.net_reconnect_attempts_spin.setValue(3)
        self.net_reconnect_attempts_spin.valueChanged.connect(self._mark_unsaved)
        reliability_layout.addRow("Reconnect Attempts:", self.net_reconnect_attempts_spin)

        self.net_reconnect_delay_spin = QDoubleSpinBox()
        self.net_reconnect_delay_spin.setRange(0.1, 60.0)
        self.net_reconnect_delay_spin.setValue(2.0)
        self.net_reconnect_delay_spin.setSuffix(" s")
        self.net_reconnect_delay_spin.valueChanged.connect(self._mark_unsaved)
        reliability_layout.addRow("Reconnect Delay:", self.net_reconnect_delay_spin)

        self.net_resume_timeout_spin = QDoubleSpinBox()
        self.net_resume_timeout_spin.setRange(0.0, 300.0)
        self.net_resume_timeout_spin.setValue(0.0)
        self.net_resume_timeout_spin.setSuffix(" s")
        self.net_resume_timeout_spin.setToolTip("Server: how long a dropped peer's slot is held for resume (0 = off).")
        self.net_resume_timeout_spin.valueChanged.connect(self._mark_unsaved)
        reliability_layout.addRow("Resume Timeout:", self.net_resume_timeout_spin)

        self.net_max_messages_spin = QSpinBox()
        self.net_max_messages_spin.setRange(0, 1000000)
        self.net_max_messages_spin.setValue(0)
        self.net_max_messages_spin.setToolTip("Anti-flood: inbound messages/s per peer before a kick (0 = off).")
        self.net_max_messages_spin.valueChanged.connect(self._mark_unsaved)
        reliability_layout.addRow("Max Messages/s:", self.net_max_messages_spin)

        self.net_max_bytes_spin = QSpinBox()
        self.net_max_bytes_spin.setRange(0, 100000000)
        self.net_max_bytes_spin.setValue(0)
        self.net_max_bytes_spin.setToolTip("Anti-flood: inbound bytes/s per peer before a kick (0 = off).")
        self.net_max_bytes_spin.valueChanged.connect(self._mark_unsaved)
        reliability_layout.addRow("Max Bytes/s:", self.net_max_bytes_spin)

        reliability_group.setLayout(reliability_layout)
        layout.addWidget(reliability_group)

        multi_group = QGroupBox("Multi-Instance Testing (Editor Play)")
        multi_layout = QFormLayout()

        self.runtime_instances_spin = QSpinBox()
        self.runtime_instances_spin.setRange(1, 16)
        self.runtime_instances_spin.setValue(1)
        self.runtime_instances_spin.setToolTip(
            "How many copies of the game the Play buttons launch at once. "
            "Use 2+ to test multiplayer locally (e.g. one host and one client).")
        self.runtime_instances_spin.valueChanged.connect(self._mark_unsaved)
        multi_layout.addRow("Instances to Launch:", self.runtime_instances_spin)

        self.runtime_unique_user_check = QCheckBox(
            "Give each instance its own user:// directory")
        self.runtime_unique_user_check.setChecked(True)
        self.runtime_unique_user_check.setToolTip(
            "Each instance gets an isolated folder under the base user data path "
            "(.../instances/<project>/instance_N), so saves, configs and networking "
            "state never collide between instances.")
        self.runtime_unique_user_check.stateChanged.connect(self._mark_unsaved)
        multi_layout.addRow("", self.runtime_unique_user_check)

        self.runtime_args_edit = QLineEdit()
        self.runtime_args_edit.setPlaceholderText("--server --port 7777")
        self.runtime_args_edit.setToolTip(
            "Extra command-line arguments forwarded to every launched instance. "
            "Game scripts can read them via get_cmdline_args(). The token "
            "{instance} is replaced with each instance's zero-based index.")
        self.runtime_args_edit.textChanged.connect(self._mark_unsaved)
        multi_layout.addRow("Runtime Args:", self.runtime_args_edit)

        multi_group.setLayout(multi_layout)
        layout.addWidget(multi_group)

        multi_info = QLabel(
            "When more than one instance is launched, each runs as a separate "
            "process so windows, input and engine state stay fully isolated.")
        multi_info.setWordWrap(True)
        layout.addWidget(multi_info)

        layout.addStretch()
        widget.setLayout(layout)
        return widget

    def _create_localization_tab(self) -> QWidget:
        """Create the Localization settings tab"""
        widget = QWidget()
        layout = QVBoxLayout()

        general_group = QGroupBox("Localization")
        form = QFormLayout()

        self.loc_enabled_check = QCheckBox("Enable localization")
        self.loc_enabled_check.stateChanged.connect(self._mark_unsaved)
        form.addRow(self.loc_enabled_check)

        self.loc_default_combo = QComboBox()
        self.loc_default_combo.setEditable(True)
        self.loc_default_combo.currentTextChanged.connect(self._mark_unsaved)
        form.addRow("Default locale:", self.loc_default_combo)

        self.loc_fallback_combo = QComboBox()
        self.loc_fallback_combo.setEditable(True)
        self.loc_fallback_combo.currentTextChanged.connect(self._mark_unsaved)
        form.addRow("Fallback locale:", self.loc_fallback_combo)

        self.loc_dir_edit = QLineEdit()
        self.loc_dir_edit.setPlaceholderText("localization")
        self.loc_dir_edit.textChanged.connect(self._mark_unsaved)
        form.addRow("Tables directory:", self.loc_dir_edit)

        self.loc_csv_check = QCheckBox("CSV mode (new tables saved as .csv)")
        self.loc_csv_check.stateChanged.connect(self._mark_unsaved)
        form.addRow(self.loc_csv_check)

        self.loc_pseudo_check = QCheckBox("Pseudolocalization (layout testing)")
        self.loc_pseudo_check.stateChanged.connect(self._mark_unsaved)
        form.addRow(self.loc_pseudo_check)

        general_group.setLayout(form)
        layout.addWidget(general_group)

        lang_group = QGroupBox("Languages")
        lang_layout = QVBoxLayout()
        self.loc_languages_list = QListWidget()
        lang_layout.addWidget(self.loc_languages_list)

        lang_buttons = QHBoxLayout()
        add_lang_btn = QPushButton("Add...")
        add_lang_btn.clicked.connect(self._add_locale)
        lang_buttons.addWidget(add_lang_btn)
        remove_lang_btn = QPushButton("Remove")
        remove_lang_btn.setProperty("danger", True)
        remove_lang_btn.clicked.connect(self._remove_locale)
        lang_buttons.addWidget(remove_lang_btn)
        lang_buttons.addStretch()
        lang_layout.addLayout(lang_buttons)
        lang_group.setLayout(lang_layout)
        layout.addWidget(lang_group)

        note_label = QLabel("Keys and translations are edited in Tools > Localization.")
        note_label.setProperty("secondary", True)
        note_label.setStyleSheet("font-style: italic; padding: 10px;")
        layout.addWidget(note_label)

        layout.addStretch()
        widget.setLayout(layout)
        return widget

    def _locale_in_list(self, code: str) -> bool:
        for i in range(self.loc_languages_list.count()):
            if self.loc_languages_list.item(i).text() == code:
                return True
        return False

    def _refresh_locale_combos(self):
        locales = [self.loc_languages_list.item(i).text()
                   for i in range(self.loc_languages_list.count())]
        for combo in (self.loc_default_combo, self.loc_fallback_combo):
            current = combo.currentText()
            combo.blockSignals(True)
            combo.clear()
            combo.addItems(locales)
            if current:
                idx = combo.findText(current)
                if idx >= 0:
                    combo.setCurrentIndex(idx)
                else:
                    combo.setEditText(current)
            combo.blockSignals(False)

    def _add_locale(self):
        text, ok = QInputDialog.getText(self, "Add Language",
                                        "Locale code (e.g. fr, pt-BR, ja):")
        if ok and text.strip():
            code = text.strip()
            if not self._locale_in_list(code):
                self.loc_languages_list.addItem(code)
                self._refresh_locale_combos()
                self._mark_unsaved()

    def _remove_locale(self):
        item = self.loc_languages_list.currentItem()
        if item:
            self.loc_languages_list.takeItem(self.loc_languages_list.row(item))
            self._refresh_locale_combos()
            self._mark_unsaved()

    def _create_splash_tab(self) -> QWidget:
        """Create the Splash Screens settings tab"""
        widget = QWidget()
        layout = QVBoxLayout()

        # Enable/Disable Group
        enable_group = QGroupBox("Splash Screen Settings")
        enable_layout = QFormLayout()

        self.splash_enabled_check = QCheckBox("Enable Splash Screens")
        self.splash_enabled_check.setToolTip("Show splash screens when the game starts")
        self.splash_enabled_check.stateChanged.connect(self._mark_unsaved)
        self.splash_enabled_check.stateChanged.connect(self._on_splash_enabled_changed)
        enable_layout.addRow(self.splash_enabled_check)

        self.splash_skip_check = QCheckBox("Allow Skip (any key/click)")
        self.splash_skip_check.setToolTip("Allow players to skip splash screens by pressing any key or clicking")
        self.splash_skip_check.stateChanged.connect(self._mark_unsaved)
        enable_layout.addRow(self.splash_skip_check)

        enable_group.setLayout(enable_layout)
        layout.addWidget(enable_group)

        # Splash Entries List Group
        entries_group = QGroupBox("Splash Screens")
        entries_layout = QVBoxLayout()

        # List widget
        from PyQt6.QtWidgets import QListWidget
        self.splash_list = QListWidget()
        self.splash_list.setMinimumHeight(150)
        self.splash_list.setMaximumHeight(200)
        self.splash_list.currentRowChanged.connect(self._on_splash_selection_changed)
        entries_layout.addWidget(self.splash_list)

        # Buttons row
        buttons_layout = QHBoxLayout()

        add_btn = QPushButton("Add")
        add_btn.setFixedWidth(80)
        add_btn.clicked.connect(self._add_splash_entry)
        buttons_layout.addWidget(add_btn)

        remove_btn = QPushButton("Remove")
        remove_btn.setFixedWidth(80)
        remove_btn.clicked.connect(self._remove_splash_entry)
        buttons_layout.addWidget(remove_btn)

        move_up_btn = QPushButton("Move Up")
        move_up_btn.setFixedWidth(80)
        move_up_btn.clicked.connect(self._move_splash_up)
        buttons_layout.addWidget(move_up_btn)

        move_down_btn = QPushButton("Move Down")
        move_down_btn.setFixedWidth(80)
        move_down_btn.clicked.connect(self._move_splash_down)
        buttons_layout.addWidget(move_down_btn)

        buttons_layout.addStretch()
        entries_layout.addLayout(buttons_layout)

        entries_group.setLayout(entries_layout)
        layout.addWidget(entries_group)

        # Entry Editor Group (shows when entry selected)
        self.splash_entry_group = QGroupBox("Selected Splash Screen")
        entry_layout = QFormLayout()

        # Image path with browse
        image_layout = QHBoxLayout()
        self.splash_image_edit = QLineEdit()
        self.splash_image_edit.setPlaceholderText("res://assets/splash/image.png")
        self.splash_image_edit.textChanged.connect(self._on_splash_entry_changed)
        image_layout.addWidget(self.splash_image_edit)

        browse_btn = QPushButton("Browse...")
        browse_btn.setFixedWidth(80)
        browse_btn.clicked.connect(self._browse_splash_image)
        image_layout.addWidget(browse_btn)
        entry_layout.addRow("Image Path:", image_layout)

        # Timing controls
        self.splash_fade_in_spin = QDoubleSpinBox()
        self.splash_fade_in_spin.setRange(0.0, 10.0)
        self.splash_fade_in_spin.setSingleStep(0.1)
        self.splash_fade_in_spin.setValue(0.3)
        self.splash_fade_in_spin.setSuffix(" s")
        self.splash_fade_in_spin.valueChanged.connect(self._on_splash_entry_changed)
        entry_layout.addRow("Fade In:", self.splash_fade_in_spin)

        self.splash_hold_spin = QDoubleSpinBox()
        self.splash_hold_spin.setRange(0.0, 30.0)
        self.splash_hold_spin.setSingleStep(0.1)
        self.splash_hold_spin.setValue(2.0)
        self.splash_hold_spin.setSuffix(" s")
        self.splash_hold_spin.valueChanged.connect(self._on_splash_entry_changed)
        entry_layout.addRow("Hold Duration:", self.splash_hold_spin)

        self.splash_fade_out_spin = QDoubleSpinBox()
        self.splash_fade_out_spin.setRange(0.0, 10.0)
        self.splash_fade_out_spin.setSingleStep(0.1)
        self.splash_fade_out_spin.setValue(0.3)
        self.splash_fade_out_spin.setSuffix(" s")
        self.splash_fade_out_spin.valueChanged.connect(self._on_splash_entry_changed)
        entry_layout.addRow("Fade Out:", self.splash_fade_out_spin)

        self.splash_entry_group.setLayout(entry_layout)
        self.splash_entry_group.setEnabled(False)
        layout.addWidget(self.splash_entry_group)

        # Initialize splash entries list
        self.splash_entries = []
        self._updating_splash_entry = False

        layout.addStretch()
        widget.setLayout(layout)
        return widget

    def _on_splash_enabled_changed(self, state):
        """Handle splash enabled checkbox change"""
        # Could disable the entries list when splash is disabled
        pass

    def _add_splash_entry(self):
        """Add a new splash screen entry"""
        entry = {
            "image_path": "",
            "fade_in_duration": 0.3,
            "hold_duration": 2.0,
            "fade_out_duration": 0.3
        }
        self.splash_entries.append(entry)
        self._refresh_splash_list()
        self.splash_list.setCurrentRow(len(self.splash_entries) - 1)
        self._mark_unsaved()

    def _remove_splash_entry(self):
        """Remove selected splash screen entry"""
        row = self.splash_list.currentRow()
        if row >= 0 and row < len(self.splash_entries):
            del self.splash_entries[row]
            self._refresh_splash_list()
            self._mark_unsaved()
            # Select previous item or first
            if self.splash_entries:
                self.splash_list.setCurrentRow(min(row, len(self.splash_entries) - 1))

    def _move_splash_up(self):
        """Move selected entry up in list"""
        row = self.splash_list.currentRow()
        if row > 0:
            self.splash_entries[row], self.splash_entries[row-1] = \
                self.splash_entries[row-1], self.splash_entries[row]
            self._refresh_splash_list()
            self.splash_list.setCurrentRow(row - 1)
            self._mark_unsaved()

    def _move_splash_down(self):
        """Move selected entry down in list"""
        row = self.splash_list.currentRow()
        if row >= 0 and row < len(self.splash_entries) - 1:
            self.splash_entries[row], self.splash_entries[row+1] = \
                self.splash_entries[row+1], self.splash_entries[row]
            self._refresh_splash_list()
            self.splash_list.setCurrentRow(row + 1)
            self._mark_unsaved()

    def _refresh_splash_list(self):
        """Refresh the splash list widget from data"""
        self.splash_list.clear()
        for i, entry in enumerate(self.splash_entries):
            path = entry.get("image_path", "")
            if path:
                name = Path(path).name
            else:
                name = f"[Entry {i+1} - No Image]"
            self.splash_list.addItem(name)

    def _on_splash_selection_changed(self, row):
        """Handle splash list selection change"""
        if row >= 0 and row < len(self.splash_entries):
            self.splash_entry_group.setEnabled(True)
            entry = self.splash_entries[row]
            self._updating_splash_entry = True
            self.splash_image_edit.setText(entry.get("image_path", ""))
            self.splash_fade_in_spin.setValue(entry.get("fade_in_duration", 0.3))
            self.splash_hold_spin.setValue(entry.get("hold_duration", 2.0))
            self.splash_fade_out_spin.setValue(entry.get("fade_out_duration", 0.3))
            self._updating_splash_entry = False
        else:
            self.splash_entry_group.setEnabled(False)

    def _on_splash_entry_changed(self):
        """Handle changes to current splash entry"""
        if self._updating_splash_entry:
            return
        row = self.splash_list.currentRow()
        if row >= 0 and row < len(self.splash_entries):
            self.splash_entries[row] = {
                "image_path": self.splash_image_edit.text(),
                "fade_in_duration": self.splash_fade_in_spin.value(),
                "hold_duration": self.splash_hold_spin.value(),
                "fade_out_duration": self.splash_fade_out_spin.value()
            }
            self._refresh_splash_list()
            self.splash_list.setCurrentRow(row)
            self._mark_unsaved()

    def _browse_splash_image(self):
        """Browse for splash image file"""
        project_dir = Path(self.project.get_directory())

        file_path, _ = QFileDialog.getOpenFileName(
            self,
            "Select Splash Image",
            str(project_dir),
            "Image Files (*.png *.jpg *.jpeg *.bmp);;All Files (*.*)"
        )
        if file_path:
            try:
                file_path = Path(file_path)
                relative_path = file_path.relative_to(project_dir)
                self.splash_image_edit.setText(f"res://{str(relative_path).replace(chr(92), '/')}")
            except ValueError:
                QMessageBox.warning(self, "Invalid Path",
                    "Splash image must be inside the project folder.")

    def _create_editor_tab(self) -> QWidget:
        """Create the Editor settings tab"""
        widget = QWidget()
        layout = QVBoxLayout()

        # Workflow Settings Group
        workflow_group = QGroupBox("Workflow Settings")
        workflow_layout = QVBoxLayout()

        # Save on play checkbox
        self.save_on_play_checkbox = QCheckBox("Save all scenes and scripts when playing")
        self.save_on_play_checkbox.setToolTip(
            "When enabled, all open scenes and scripts will be automatically saved\n"
            "before playing the scene or game. This ensures you're always testing\n"
            "the latest changes."
        )
        self.save_on_play_checkbox.stateChanged.connect(self._mark_unsaved)
        workflow_layout.addWidget(self.save_on_play_checkbox)

        # Info text
        info_label = QLabel(
            "Automatically saves all open files before running the game or scene.\n"
            "This prevents testing outdated code or scene data."
        )
        info_label.setProperty("secondary", True)
        info_label.setWordWrap(True)
        info_label.setStyleSheet("padding: 10px; background-color: rgba(255, 255, 255, 0.05); border-radius: 4px; margin-top: 5px;")
        workflow_layout.addWidget(info_label)

        workflow_group.setLayout(workflow_layout)
        layout.addWidget(workflow_group)

        # Theme Settings Group
        theme_group = QGroupBox("Theme Settings")
        theme_layout = QVBoxLayout()

        # Description
        desc_label = QLabel("Customize the editor's appearance and theme settings.")
        desc_label.setProperty("secondary", True)
        desc_label.setWordWrap(True)
        theme_layout.addWidget(desc_label)

        # Theme editor button
        theme_btn = QPushButton("Open Theme Editor...")
        theme_btn.setFixedWidth(200)
        theme_btn.clicked.connect(self._open_theme_editor)
        theme_layout.addWidget(theme_btn)

        # Info text
        theme_info_label = QLabel(
            "The Theme Editor allows you to:\n"
            "  • Customize all editor colors\n"
            "  • Adjust spacing and padding\n"
            "  • Export and import custom themes\n"
            "  • Preview changes in real-time"
        )
        theme_info_label.setStyleSheet("padding: 10px; background-color: rgba(255, 255, 255, 0.05); border-radius: 4px;")
        theme_layout.addWidget(theme_info_label)

        theme_group.setLayout(theme_layout)
        layout.addWidget(theme_group)

        layout.addStretch()
        widget.setLayout(layout)
        return widget
    
    def _open_theme_editor(self):
        """Open the theme editor dialog"""
        from dialogs import ThemeEditorDialog
        dialog = ThemeEditorDialog(self)
        # Connect theme_changed signal to notify the main editor
        dialog.theme_changed.connect(self._on_theme_changed)
        dialog.exec()

    def _on_theme_changed(self, theme):
        """Handle theme change - notify the main editor to update panels"""
        # Find the main editor (our parent)
        main_editor = self.parent()
        if main_editor and hasattr(main_editor, 'panels'):
            # Update scene tree panel icons
            if 'scene_tree' in main_editor.panels:
                main_editor.panels['scene_tree'].on_theme_changed()
    
    def _load_settings(self):
        """Load settings from project data into UI"""
        # General
        self.project_name_edit.setText(self.project.name)
        self.creator_edit.setText(self.project.creator)
        self.version_edit.setText(self.project.version)
        self.icon_edit.setText(self.project.icon_path)
        self.main_scene_edit.setText(self.project.main_scene)
        self.default_font_edit.setText(self.project.default_font)
        self.default_theme_edit.setText(self.project.default_theme)

        # Window
        self.window_width_spin.setValue(self.project.window_width)
        self.window_height_spin.setValue(self.project.window_height)
        self.fullscreen_check.setChecked(self.project.fullscreen)
        self.resizable_check.setChecked(self.project.window_resizable)
        self.borderless_check.setChecked(self.project.window_borderless)
        self.window_size_override_check.setChecked(self.project.window_size_override)
        self.window_size_override_width_spin.setValue(self.project.window_size_override_width)
        self.window_size_override_height_spin.setValue(self.project.window_size_override_height)
        self._on_window_size_override_toggled()

        # Graphics
        self.vsync_check.setChecked(self.project.vsync)
        self.target_fps_spin.setValue(self.project.target_fps)

        # Scale mode
        scale_mode_map = {"letterbox": 0, "stretch": 1, "crop": 2, "ignore": 3}
        self.scale_mode_combo.setCurrentIndex(scale_mode_map.get(self.project.scale_mode.lower(), 0))

        # Texture filtering
        texture_filtering_map = {"cubic": 0, "bilinear": 1, "nearest": 2}
        self.texture_filtering_combo.setCurrentIndex(texture_filtering_map.get(self.project.texture_filtering.lower(), 1))

        # Clear color
        cc = list(self.project.clear_color) if self.project.clear_color else [0.1, 0.1, 0.1, 1.0]
        while len(cc) < 4:
            cc.append(1.0)
        self._clear_color = cc[:4]
        self._update_clear_color_swatch()

        # Physics
        g2 = self.project.gravity_2d if self.project.gravity_2d else [0.0, -980.0]
        self.gravity_2d_x_spin.setValue(float(g2[0]) if len(g2) > 0 else 0.0)
        self.gravity_2d_y_spin.setValue(float(g2[1]) if len(g2) > 1 else -980.0)
        g3 = self.project.gravity_3d if self.project.gravity_3d else [0.0, -9.8, 0.0]
        self.gravity_3d_x_spin.setValue(float(g3[0]) if len(g3) > 0 else 0.0)
        self.gravity_3d_y_spin.setValue(float(g3[1]) if len(g3) > 1 else -9.8)
        self.gravity_3d_z_spin.setValue(float(g3[2]) if len(g3) > 2 else 0.0)
        self.physics_fps_spin.setValue(int(round(self.project.physics_tick_rate)))

        # Audio
        self.master_volume_spin.setValue(float(self.project.master_volume))
        self.music_volume_spin.setValue(float(self.project.music_volume))
        self.sfx_volume_spin.setValue(float(self.project.sfx_volume))

        # Networking
        transport_map = {"enet": 0, "websocket": 1, "loopback": 2}
        self.net_transport_combo.setCurrentIndex(
            transport_map.get(str(self.project.networking_transport).lower(), 0))
        self.net_port_spin.setValue(int(self.project.networking_default_port))
        self.net_max_peers_spin.setValue(int(self.project.networking_max_peers))
        self.net_tick_rate_spin.setValue(float(self.project.networking_tick_rate))
        self.net_interp_delay_spin.setValue(float(self.project.networking_interp_delay_ms))
        self.net_keyframe_interval_spin.setValue(float(self.project.networking_keyframe_interval_ms))
        self.net_ping_interval_spin.setValue(float(self.project.networking_ping_interval_seconds))
        self.net_interest_radius_spin.setValue(float(self.project.networking_interest_radius))
        self.net_protocol_spin.setValue(int(self.project.networking_protocol_version))
        self.net_game_id_edit.setText(str(self.project.networking_game_id))
        self.net_lan_discovery_check.setChecked(bool(self.project.networking_enable_lan_discovery))
        self.net_discovery_port_spin.setValue(int(self.project.networking_discovery_port))
        self.net_server_name_edit.setText(str(self.project.networking_server_name))
        self.net_prediction_check.setChecked(bool(getattr(self.project, "networking_enable_prediction", True)))
        self.net_input_redundancy_spin.setValue(int(getattr(self.project, "networking_input_redundancy", 3)))
        self.net_auto_reconnect_check.setChecked(bool(getattr(self.project, "networking_auto_reconnect", False)))
        self.net_reconnect_attempts_spin.setValue(int(getattr(self.project, "networking_reconnect_attempts", 3)))
        self.net_reconnect_delay_spin.setValue(float(getattr(self.project, "networking_reconnect_delay_seconds", 2.0)))
        self.net_resume_timeout_spin.setValue(float(getattr(self.project, "networking_resume_timeout_seconds", 0.0)))
        self.net_max_messages_spin.setValue(int(getattr(self.project, "networking_max_messages_per_second", 0)))
        self.net_max_bytes_spin.setValue(int(getattr(self.project, "networking_max_bytes_per_second", 0)))

        # Multi-instance testing
        self.runtime_instances_spin.setValue(
            max(1, int(getattr(self.project, "runtime_instance_count", 1))))
        self.runtime_unique_user_check.setChecked(
            bool(getattr(self.project, "runtime_unique_user_dir", True)))
        self.runtime_args_edit.setText(str(getattr(self.project, "runtime_args", "")))

        # Editor
        self.save_on_play_checkbox.setChecked(self.project.save_on_play)

        # Collision Layers
        for i, edit in enumerate(self.layer_2d_edits):
            name = self.project.collision_layers_2d[i] if i < len(self.project.collision_layers_2d) else f"Layer {i+1}"
            # Only set text if it's not the default "Layer X" format
            if name != f"Layer {i+1}":
                edit.setText(name)
            else:
                edit.setText("")

        for i, edit in enumerate(self.layer_3d_edits):
            name = self.project.collision_layers_3d[i] if i < len(self.project.collision_layers_3d) else f"Layer {i+1}"
            # Only set text if it's not the default "Layer X" format
            if name != f"Layer {i+1}":
                edit.setText(name)
            else:
                edit.setText("")

        # Splash Screens
        self.splash_enabled_check.setChecked(self.project.splash_screens_enabled)
        self.splash_skip_check.setChecked(self.project.splash_screens_allow_skip)
        self.splash_entries = list(self.project.splash_screen_entries)  # Copy the list
        self._refresh_splash_list()
        if self.splash_entries:
            self.splash_list.setCurrentRow(0)

        # Localization
        loc = getattr(self.project, "localization", None) or {}
        self.loc_enabled_check.setChecked(loc.get("enabled", True))
        self.loc_dir_edit.setText(loc.get("tables_dir", "localization"))
        self.loc_csv_check.setChecked(loc.get("csv_mode", False))
        self.loc_pseudo_check.setChecked(loc.get("pseudolocalization", False))
        self.loc_languages_list.clear()
        for code in loc.get("locales", ["en"]):
            self.loc_languages_list.addItem(code)
        self._refresh_locale_combos()
        self.loc_default_combo.setEditText(loc.get("default_locale", "en"))
        self.loc_fallback_combo.setEditText(loc.get("fallback_locale", "en"))

    def _save_settings(self):
        """Save settings from UI into project data"""
        # General
        self.project.name = self.project_name_edit.text()
        self.project.creator = self.creator_edit.text()
        self.project.version = self.version_edit.text()
        self.project.icon_path = self.icon_edit.text()
        self.project.main_scene = self.main_scene_edit.text()
        self.project.default_font = self.default_font_edit.text()
        self.project.default_theme = self.default_theme_edit.text()

        # Window
        self.project.window_width = self.window_width_spin.value()
        self.project.window_height = self.window_height_spin.value()
        self.project.fullscreen = self.fullscreen_check.isChecked()
        self.project.window_resizable = self.resizable_check.isChecked()
        self.project.window_borderless = self.borderless_check.isChecked()
        self.project.window_size_override = self.window_size_override_check.isChecked()
        self.project.window_size_override_width = self.window_size_override_width_spin.value()
        self.project.window_size_override_height = self.window_size_override_height_spin.value()

        # Graphics
        self.project.vsync = self.vsync_check.isChecked()
        self.project.target_fps = self.target_fps_spin.value()

        # Scale mode
        scale_mode_values = ["letterbox", "stretch", "crop", "ignore"]
        self.project.scale_mode = scale_mode_values[self.scale_mode_combo.currentIndex()]

        # Texture filtering
        texture_filtering_values = ["cubic", "bilinear", "nearest"]
        self.project.texture_filtering = texture_filtering_values[self.texture_filtering_combo.currentIndex()]

        # Clear color
        self.project.clear_color = list(self._clear_color)

        # Physics
        self.project.gravity_2d = [self.gravity_2d_x_spin.value(), self.gravity_2d_y_spin.value()]
        self.project.gravity_3d = [
            self.gravity_3d_x_spin.value(),
            self.gravity_3d_y_spin.value(),
            self.gravity_3d_z_spin.value(),
        ]
        self.project.physics_tick_rate = float(self.physics_fps_spin.value())

        # Audio
        self.project.master_volume = self.master_volume_spin.value()
        self.project.music_volume = self.music_volume_spin.value()
        self.project.sfx_volume = self.sfx_volume_spin.value()

        # Networking
        transport_names = ["enet", "websocket", "loopback"]
        self.project.networking_transport = transport_names[self.net_transport_combo.currentIndex()]
        self.project.networking_default_port = self.net_port_spin.value()
        self.project.networking_max_peers = self.net_max_peers_spin.value()
        self.project.networking_tick_rate = self.net_tick_rate_spin.value()
        self.project.networking_interp_delay_ms = self.net_interp_delay_spin.value()
        self.project.networking_keyframe_interval_ms = self.net_keyframe_interval_spin.value()
        self.project.networking_ping_interval_seconds = self.net_ping_interval_spin.value()
        self.project.networking_interest_radius = self.net_interest_radius_spin.value()
        self.project.networking_protocol_version = self.net_protocol_spin.value()
        self.project.networking_game_id = self.net_game_id_edit.text().strip() or "lupine-game"
        self.project.networking_enable_lan_discovery = self.net_lan_discovery_check.isChecked()
        self.project.networking_discovery_port = self.net_discovery_port_spin.value()
        self.project.networking_server_name = self.net_server_name_edit.text().strip() or "Lupine Server"
        self.project.networking_enable_prediction = self.net_prediction_check.isChecked()
        self.project.networking_input_redundancy = self.net_input_redundancy_spin.value()
        self.project.networking_auto_reconnect = self.net_auto_reconnect_check.isChecked()
        self.project.networking_reconnect_attempts = self.net_reconnect_attempts_spin.value()
        self.project.networking_reconnect_delay_seconds = self.net_reconnect_delay_spin.value()
        self.project.networking_resume_timeout_seconds = self.net_resume_timeout_spin.value()
        self.project.networking_max_messages_per_second = self.net_max_messages_spin.value()
        self.project.networking_max_bytes_per_second = self.net_max_bytes_spin.value()

        # Multi-instance testing
        self.project.runtime_instance_count = self.runtime_instances_spin.value()
        self.project.runtime_unique_user_dir = self.runtime_unique_user_check.isChecked()
        self.project.runtime_args = self.runtime_args_edit.text().strip()

        # Editor
        self.project.save_on_play = self.save_on_play_checkbox.isChecked()

        # Collision Layers
        for i, edit in enumerate(self.layer_2d_edits):
            text = edit.text().strip()
            self.project.collision_layers_2d[i] = text if text else f"Layer {i+1}"

        for i, edit in enumerate(self.layer_3d_edits):
            text = edit.text().strip()
            self.project.collision_layers_3d[i] = text if text else f"Layer {i+1}"

        # Splash Screens
        self.project.splash_screens_enabled = self.splash_enabled_check.isChecked()
        self.project.splash_screens_allow_skip = self.splash_skip_check.isChecked()
        self.project.splash_screen_entries = list(self.splash_entries)  # Copy the list

        # Localization
        loc_locales = [self.loc_languages_list.item(i).text()
                       for i in range(self.loc_languages_list.count())]
        if not loc_locales:
            loc_locales = ["en"]
        self.project.localization = {
            "enabled": self.loc_enabled_check.isChecked(),
            "default_locale": self.loc_default_combo.currentText().strip() or "en",
            "fallback_locale": self.loc_fallback_combo.currentText().strip() or "en",
            "locales": loc_locales,
            "tables_dir": self.loc_dir_edit.text().strip() or "localization",
            "csv_mode": self.loc_csv_check.isChecked(),
            "pseudolocalization": self.loc_pseudo_check.isChecked(),
        }
        # Also write localization.json (the file the engine reads directly).
        try:
            loc_path = Path(self.project.get_directory()) / "localization.json"
            with open(loc_path, "w", encoding="utf-8") as loc_file:
                json.dump({"lupine_localization": 1, **self.project.localization},
                          loc_file, indent=2, ensure_ascii=False)
        except Exception as exc:
            print(f"Failed to write localization.json: {exc}")

        # Save to file
        from project_file import ProjectFile
        if ProjectFile.save_project(self.project):
            self.has_unsaved_changes = False
            self.settings_saved.emit(self.project)
            return True
        return False
    
    def _mark_unsaved(self):
        """Mark that there are unsaved changes"""
        self.has_unsaved_changes = True

    def _update_clear_color_swatch(self):
        """Refresh the clear-color button swatch from self._clear_color"""
        r, g, b, a = (self._clear_color + [1.0, 1.0, 1.0, 1.0])[:4]
        qc = QColor(int(round(r * 255)), int(round(g * 255)),
                    int(round(b * 255)), int(round(a * 255)))
        self.clear_color_button.setStyleSheet(
            f"background-color: rgba({qc.red()}, {qc.green()}, {qc.blue()}, {qc.alpha()});"
            " border: 1px solid #888;"
        )

    def _pick_clear_color(self):
        """Open a color picker for the background clear color"""
        r, g, b, a = (self._clear_color + [1.0, 1.0, 1.0, 1.0])[:4]
        initial = QColor(int(round(r * 255)), int(round(g * 255)),
                         int(round(b * 255)), int(round(a * 255)))
        color = QColorDialog.getColor(
            initial, self, "Select Clear Color",
            QColorDialog.ColorDialogOption.ShowAlphaChannel
        )
        if color.isValid():
            self._clear_color = [
                color.red() / 255.0,
                color.green() / 255.0,
                color.blue() / 255.0,
                color.alpha() / 255.0,
            ]
            self._update_clear_color_swatch()
            self._mark_unsaved()
    
    def _browse_icon(self):
        """Browse for project icon file"""
        project_dir = Path(self.project.get_directory())

        file_path, _ = QFileDialog.getOpenFileName(
            self,
            "Select Project Icon",
            str(project_dir),
            "Image Files (*.png *.jpg *.jpeg *.bmp *.ico *.svg);;All Files (*.*)"
        )

        if file_path:
            file_path = Path(file_path)
            try:
                # Already inside the project: reference it in place.
                relative_path = file_path.relative_to(project_dir)
                self.icon_edit.setText(f"res://{str(relative_path).replace(chr(92), '/')}")
            except ValueError:
                # Outside the project: copy it in as the project icon so the
                # icon always travels with the project (and exports).
                import shutil
                dest_name = f"icon{file_path.suffix.lower() or '.png'}"
                dest_path = project_dir / dest_name
                try:
                    shutil.copy2(file_path, dest_path)
                except Exception as e:
                    QMessageBox.warning(self, "Copy Failed",
                        f"Could not copy the icon into the project:\n{e}")
                    return
                self.icon_edit.setText(f"res://{dest_name}")

            self._mark_unsaved()

    def _browse_default_font(self):
        """Browse for default font file"""
        project_dir = Path(self.project.get_directory())

        file_path, _ = QFileDialog.getOpenFileName(
            self,
            "Select Default Font",
            str(project_dir),
            "Font Files (*.ttf *.otf);;All Files (*.*)"
        )

        if file_path:
            # Convert to res:// path if inside project directory
            try:
                file_path = Path(file_path)
                relative_path = file_path.relative_to(project_dir)
                self.default_font_edit.setText(f"res://{str(relative_path).replace(chr(92), '/')}")
            except ValueError:
                # File is outside project directory, warn user
                QMessageBox.warning(self, "Invalid Path",
                    "Default font must be inside the project folder.")
                return

            self._mark_unsaved()

    def _browse_default_theme(self):
        """Browse for the default UI theme file"""
        project_dir = Path(self.project.get_directory())

        file_path, _ = QFileDialog.getOpenFileName(
            self,
            "Select Default UI Theme",
            str(project_dir),
            "UI Theme Files (*.uitheme);;All Files (*.*)"
        )

        if file_path:
            try:
                file_path = Path(file_path)
                relative_path = file_path.relative_to(project_dir)
                self.default_theme_edit.setText(f"res://{str(relative_path).replace(chr(92), '/')}")
            except ValueError:
                QMessageBox.warning(self, "Invalid Path",
                    "Default theme must be inside the project folder.")
                return

            self._mark_unsaved()

    def _on_tab_changed(self, index):
        """Handle tab change"""
        pass
    
    def _on_save(self):
        """Save settings"""
        if self._save_settings():
            QMessageBox.information(self, "Settings Saved", "Project settings have been saved successfully.")
    
    def _on_save_and_close(self):
        """Save settings and close dialog"""
        if self._save_settings():
            self.accept()
    
    def _on_close(self):
        """Close dialog with unsaved changes check"""
        if self.has_unsaved_changes:
            reply = QMessageBox.question(
                self,
                "Unsaved Changes",
                "You have unsaved changes. Do you want to close without saving?",
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
                QMessageBox.StandardButton.No
            )
            
            if reply == QMessageBox.StandardButton.Yes:
                self.reject()
        else:
            self.reject()
    
    def closeEvent(self, event):
        """Handle window close event"""
        if self.has_unsaved_changes:
            reply = QMessageBox.question(
                self,
                "Unsaved Changes",
                "You have unsaved changes. Do you want to close without saving?",
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
                QMessageBox.StandardButton.No
            )
            
            if reply == QMessageBox.StandardButton.Yes:
                event.accept()
            else:
                event.ignore()
        else:
            event.accept()
