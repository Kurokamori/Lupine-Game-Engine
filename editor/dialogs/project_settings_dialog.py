"""
Project Settings Dialog
Godot-style project settings editor with multiple tabs
"""

from PyQt6.QtWidgets import (QDialog, QVBoxLayout, QHBoxLayout, QTabWidget,
                             QPushButton, QWidget, QLabel, QLineEdit, QSpinBox,
                             QCheckBox, QComboBox, QGroupBox, QFormLayout,
                             QMessageBox, QScrollArea, QDoubleSpinBox, QFileDialog)
from PyQt6.QtCore import Qt, pyqtSignal
from pathlib import Path
import sys
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
        
        layout.addStretch()
        widget.setLayout(layout)
        return widget
    
    def _create_window_tab(self) -> QWidget:
        """Create the Window settings tab"""
        widget = QWidget()
        layout = QVBoxLayout()
        
        # Window Size Group
        size_group = QGroupBox("Window Size")
        size_layout = QFormLayout()
        
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
        self.backend_combo.currentIndexChanged.connect(self._mark_unsaved)
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

        display_group.setLayout(display_layout)
        layout.addWidget(display_group)

        # Quality Group
        quality_group = QGroupBox("Quality Settings")
        quality_layout = QFormLayout()

        self.msaa_combo = QComboBox()
        self.msaa_combo.addItems(["Disabled", "2x", "4x", "8x", "16x"])
        self.msaa_combo.currentIndexChanged.connect(self._mark_unsaved)
        quality_layout.addRow("MSAA:", self.msaa_combo)

        self.shadow_quality_combo = QComboBox()
        self.shadow_quality_combo.addItems(["Low", "Medium", "High", "Ultra"])
        self.shadow_quality_combo.setCurrentIndex(2)
        self.shadow_quality_combo.currentIndexChanged.connect(self._mark_unsaved)
        quality_layout.addRow("Shadow Quality:", self.shadow_quality_combo)

        self.texture_quality_combo = QComboBox()
        self.texture_quality_combo.addItems(["Low", "Medium", "High", "Ultra"])
        self.texture_quality_combo.setCurrentIndex(3)
        self.texture_quality_combo.currentIndexChanged.connect(self._mark_unsaved)
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
        self.music_volume_spin.setValue(0.8)
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
        
        # Audio Settings Group
        audio_group = QGroupBox("Audio Settings")
        audio_layout = QFormLayout()
        
        self.audio_backend_combo = QComboBox()
        self.audio_backend_combo.addItems(["Auto", "OpenAL", "XAudio2", "WASAPI"])
        self.audio_backend_combo.currentIndexChanged.connect(self._mark_unsaved)
        audio_layout.addRow("Audio Backend:", self.audio_backend_combo)
        
        self.sample_rate_combo = QComboBox()
        self.sample_rate_combo.addItems(["44100 Hz", "48000 Hz", "96000 Hz"])
        self.sample_rate_combo.setCurrentIndex(1)
        self.sample_rate_combo.currentIndexChanged.connect(self._mark_unsaved)
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
        self.gravity_2d_x_spin.setRange(-1000.0, 1000.0)
        self.gravity_2d_x_spin.setValue(0.0)
        self.gravity_2d_x_spin.valueChanged.connect(self._mark_unsaved)
        physics_2d_layout.addRow("Gravity X:", self.gravity_2d_x_spin)
        
        self.gravity_2d_y_spin = QDoubleSpinBox()
        self.gravity_2d_y_spin.setRange(-1000.0, 1000.0)
        self.gravity_2d_y_spin.setValue(98.0)
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
        
        layout.addStretch()
        widget.setLayout(layout)
        return widget
    
    def _create_input_tab(self) -> QWidget:
        """Create the Input settings tab"""
        widget = QWidget()
        layout = QVBoxLayout()
        
        # Input Group
        input_group = QGroupBox("Input Settings")
        input_layout = QFormLayout()
        
        self.mouse_sensitivity_spin = QDoubleSpinBox()
        self.mouse_sensitivity_spin.setRange(0.1, 10.0)
        self.mouse_sensitivity_spin.setSingleStep(0.1)
        self.mouse_sensitivity_spin.setValue(1.0)
        self.mouse_sensitivity_spin.valueChanged.connect(self._mark_unsaved)
        input_layout.addRow("Mouse Sensitivity:", self.mouse_sensitivity_spin)
        
        self.invert_y_check = QCheckBox("Invert Y Axis")
        self.invert_y_check.stateChanged.connect(self._mark_unsaved)
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
        dialog.exec()
    
    def _load_settings(self):
        """Load settings from project data into UI"""
        # General
        self.project_name_edit.setText(self.project.name)
        self.creator_edit.setText(self.project.creator)
        self.version_edit.setText(self.project.version)
        self.icon_edit.setText(self.project.icon_path)
        self.main_scene_edit.setText(self.project.main_scene)

        # Window
        self.window_width_spin.setValue(self.project.window_width)
        self.window_height_spin.setValue(self.project.window_height)
        self.fullscreen_check.setChecked(self.project.fullscreen)

        # Graphics
        self.vsync_check.setChecked(self.project.vsync)
        self.target_fps_spin.setValue(self.project.target_fps)

        # Scale mode
        scale_mode_map = {"letterbox": 0, "stretch": 1, "crop": 2, "ignore": 3}
        self.scale_mode_combo.setCurrentIndex(scale_mode_map.get(self.project.scale_mode.lower(), 0))

        # Texture filtering
        texture_filtering_map = {"cubic": 0, "bilinear": 1, "nearest": 2}
        self.texture_filtering_combo.setCurrentIndex(texture_filtering_map.get(self.project.texture_filtering.lower(), 1))

        # Editor
        self.save_on_play_checkbox.setChecked(self.project.save_on_play)
    
    def _save_settings(self):
        """Save settings from UI into project data"""
        # General
        self.project.name = self.project_name_edit.text()
        self.project.creator = self.creator_edit.text()
        self.project.version = self.version_edit.text()
        self.project.icon_path = self.icon_edit.text()
        self.project.main_scene = self.main_scene_edit.text()

        # Window
        self.project.window_width = self.window_width_spin.value()
        self.project.window_height = self.window_height_spin.value()
        self.project.fullscreen = self.fullscreen_check.isChecked()

        # Graphics
        self.project.vsync = self.vsync_check.isChecked()
        self.project.target_fps = self.target_fps_spin.value()

        # Scale mode
        scale_mode_values = ["letterbox", "stretch", "crop", "ignore"]
        self.project.scale_mode = scale_mode_values[self.scale_mode_combo.currentIndex()]

        # Texture filtering
        texture_filtering_values = ["cubic", "bilinear", "nearest"]
        self.project.texture_filtering = texture_filtering_values[self.texture_filtering_combo.currentIndex()]

        # Editor
        self.project.save_on_play = self.save_on_play_checkbox.isChecked()

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
            # Convert to relative path if inside project directory
            try:
                file_path = Path(file_path)
                relative_path = file_path.relative_to(project_dir)
                self.icon_edit.setText(str(relative_path).replace('\\', '/'))
            except ValueError:
                # File is outside project directory, use absolute path
                self.icon_edit.setText(str(file_path).replace('\\', '/'))
            
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
