"""
Lupine Engine Export Dialog

PyQt6 dialog for exporting games to various platforms.
"""

import os
from pathlib import Path
from typing import Optional, Callable

from PyQt6.QtWidgets import (
    QDialog, QVBoxLayout, QHBoxLayout, QGridLayout,
    QLabel, QLineEdit, QPushButton, QComboBox, QCheckBox,
    QSpinBox, QGroupBox, QTabWidget, QWidget, QFileDialog,
    QProgressBar, QTextEdit, QListWidget, QListWidgetItem,
    QSplitter, QFrame, QMessageBox, QSizePolicy
)
from PyQt6.QtCore import Qt, QThread, pyqtSignal, QSize
from PyQt6.QtGui import QIcon, QFont

from .export_manager import (
    ExportManager, ExportPreset, ExportPlatform, ExportResult,
    get_current_platform, create_default_presets
)


class ExportWorker(QThread):
    """Background worker for export operations"""
    progress = pyqtSignal(str, float)
    finished = pyqtSignal(object)

    def __init__(self, manager: ExportManager, project_path: str, preset: ExportPreset):
        super().__init__()
        self.manager = manager
        self.project_path = project_path
        self.preset = preset

    def run(self):
        def progress_callback(message: str, value: float):
            self.progress.emit(message, value)

        self.manager.progress_callback = progress_callback
        result = self.manager.export_project(self.project_path, self.preset)
        self.manager.progress_callback = None
        self.finished.emit(result)


class ExportDialog(QDialog):
    """Main export dialog"""

    def __init__(self, project_path: str, project_data: dict, parent=None):
        super().__init__(parent)
        self.project_path = project_path
        self.project_data = project_data
        self.project_name = project_data.get('project', {}).get('name', 'Game')

        self.manager = ExportManager()
        self.current_preset: Optional[ExportPreset] = None
        self.worker: Optional[ExportWorker] = None

        self.setWindowTitle(f"Export Project - {self.project_name}")
        self.setMinimumSize(800, 600)

        self._setup_ui()
        self._load_presets()
        self._update_template_status()

    def _setup_ui(self):
        """Setup the dialog UI"""
        layout = QVBoxLayout(self)

        # Main splitter
        splitter = QSplitter(Qt.Orientation.Horizontal)

        # Left panel - Presets list
        left_panel = QWidget()
        left_layout = QVBoxLayout(left_panel)
        left_layout.setContentsMargins(0, 0, 0, 0)

        presets_label = QLabel("Export Presets")
        presets_label.setFont(QFont("", 10, QFont.Weight.Bold))
        left_layout.addWidget(presets_label)

        self.presets_list = QListWidget()
        self.presets_list.currentItemChanged.connect(self._on_preset_selected)
        left_layout.addWidget(self.presets_list)

        # Preset buttons
        preset_buttons = QHBoxLayout()
        self.add_preset_btn = QPushButton("+")
        self.add_preset_btn.setFixedWidth(30)
        self.add_preset_btn.setToolTip("Add new preset")
        self.add_preset_btn.clicked.connect(self._add_preset)

        self.remove_preset_btn = QPushButton("-")
        self.remove_preset_btn.setFixedWidth(30)
        self.remove_preset_btn.setToolTip("Remove preset")
        self.remove_preset_btn.clicked.connect(self._remove_preset)

        self.duplicate_preset_btn = QPushButton("Duplicate")
        self.duplicate_preset_btn.clicked.connect(self._duplicate_preset)

        preset_buttons.addWidget(self.add_preset_btn)
        preset_buttons.addWidget(self.remove_preset_btn)
        preset_buttons.addWidget(self.duplicate_preset_btn)
        preset_buttons.addStretch()
        left_layout.addLayout(preset_buttons)

        left_panel.setMaximumWidth(250)
        splitter.addWidget(left_panel)

        # Right panel - Preset configuration
        right_panel = QWidget()
        right_layout = QVBoxLayout(right_panel)
        right_layout.setContentsMargins(0, 0, 0, 0)

        # Tabs for configuration
        self.tabs = QTabWidget()

        # General tab
        general_tab = QWidget()
        general_layout = QVBoxLayout(general_tab)
        self._setup_general_tab(general_layout)
        self.tabs.addTab(general_tab, "General")

        # Platform-specific tab
        platform_tab = QWidget()
        platform_layout = QVBoxLayout(platform_tab)
        self._setup_platform_tab(platform_layout)
        self.tabs.addTab(platform_tab, "Platform")

        # Resources tab
        resources_tab = QWidget()
        resources_layout = QVBoxLayout(resources_tab)
        self._setup_resources_tab(resources_layout)
        self.tabs.addTab(resources_tab, "Resources")

        right_layout.addWidget(self.tabs)

        # Template status
        self.template_status = QLabel()
        self.template_status.setStyleSheet("padding: 5px; border-radius: 3px;")
        right_layout.addWidget(self.template_status)

        splitter.addWidget(right_panel)
        splitter.setSizes([200, 600])

        layout.addWidget(splitter)

        # Progress section
        progress_group = QGroupBox("Export Progress")
        progress_layout = QVBoxLayout(progress_group)

        self.progress_bar = QProgressBar()
        self.progress_bar.setVisible(False)
        progress_layout.addWidget(self.progress_bar)

        self.progress_label = QLabel("Ready to export")
        progress_layout.addWidget(self.progress_label)

        self.log_output = QTextEdit()
        self.log_output.setReadOnly(True)
        self.log_output.setMaximumHeight(100)
        self.log_output.setVisible(False)
        progress_layout.addWidget(self.log_output)

        layout.addWidget(progress_group)

        # Buttons
        buttons_layout = QHBoxLayout()

        self.export_btn = QPushButton("Export Project")
        self.export_btn.setDefault(True)
        self.export_btn.clicked.connect(self._start_export)
        self.export_btn.setMinimumHeight(35)

        self.export_all_btn = QPushButton("Export All Presets")
        self.export_all_btn.clicked.connect(self._export_all)

        self.close_btn = QPushButton("Close")
        self.close_btn.clicked.connect(self.close)

        buttons_layout.addWidget(self.export_all_btn)
        buttons_layout.addStretch()
        buttons_layout.addWidget(self.export_btn)
        buttons_layout.addWidget(self.close_btn)

        layout.addLayout(buttons_layout)

    def _setup_general_tab(self, layout: QVBoxLayout):
        """Setup the general settings tab"""
        # Preset name
        name_layout = QHBoxLayout()
        name_layout.addWidget(QLabel("Preset Name:"))
        self.preset_name_edit = QLineEdit()
        self.preset_name_edit.textChanged.connect(self._on_preset_modified)
        name_layout.addWidget(self.preset_name_edit)
        layout.addLayout(name_layout)

        # Platform selection
        platform_layout = QHBoxLayout()
        platform_layout.addWidget(QLabel("Platform:"))
        self.platform_combo = QComboBox()
        for platform in ExportPlatform:
            self.platform_combo.addItem(platform.display_name, platform)
        self.platform_combo.currentIndexChanged.connect(self._on_platform_changed)
        platform_layout.addWidget(self.platform_combo)
        platform_layout.addStretch()
        layout.addLayout(platform_layout)

        # Output path
        output_group = QGroupBox("Output")
        output_layout = QGridLayout(output_group)

        output_layout.addWidget(QLabel("Executable Name:"), 0, 0)
        self.exe_name_edit = QLineEdit()
        self.exe_name_edit.setPlaceholderText("MyGame")
        self.exe_name_edit.textChanged.connect(self._on_preset_modified)
        output_layout.addWidget(self.exe_name_edit, 0, 1)

        output_layout.addWidget(QLabel("Output Path:"), 1, 0)
        self.output_path_edit = QLineEdit()
        self.output_path_edit.textChanged.connect(self._on_preset_modified)
        output_layout.addWidget(self.output_path_edit, 1, 1)

        browse_btn = QPushButton("Browse...")
        browse_btn.clicked.connect(self._browse_output)
        output_layout.addWidget(browse_btn, 1, 2)

        layout.addWidget(output_group)

        # Graphics backend
        graphics_group = QGroupBox("Graphics")
        graphics_layout = QHBoxLayout(graphics_group)

        graphics_layout.addWidget(QLabel("Backend:"))
        self.backend_combo = QComboBox()
        self.backend_combo.addItems(["OpenGL", "Vulkan", "DirectX 11", "DirectX 12", "Metal", "WebGL"])
        self.backend_combo.currentIndexChanged.connect(self._on_preset_modified)
        graphics_layout.addWidget(self.backend_combo)
        graphics_layout.addStretch()

        layout.addWidget(graphics_group)

        # Pack options
        pack_group = QGroupBox("Pack Options")
        pack_layout = QVBoxLayout(pack_group)

        self.embed_pack_check = QCheckBox("Embed pack file in executable")
        self.embed_pack_check.setToolTip("Creates a single executable file with all game data embedded")
        self.embed_pack_check.stateChanged.connect(self._on_preset_modified)
        pack_layout.addWidget(self.embed_pack_check)

        compress_layout = QHBoxLayout()
        self.compress_check = QCheckBox("Compress assets")
        self.compress_check.stateChanged.connect(self._on_preset_modified)
        compress_layout.addWidget(self.compress_check)

        compress_layout.addWidget(QLabel("Level:"))
        self.compress_level_spin = QSpinBox()
        self.compress_level_spin.setRange(1, 9)
        self.compress_level_spin.setValue(9)
        self.compress_level_spin.valueChanged.connect(self._on_preset_modified)
        compress_layout.addWidget(self.compress_level_spin)
        compress_layout.addStretch()
        pack_layout.addLayout(compress_layout)

        layout.addWidget(pack_group)

        layout.addStretch()

    def _setup_platform_tab(self, layout: QVBoxLayout):
        """Setup platform-specific settings tab"""
        # Windows settings
        self.windows_group = QGroupBox("Windows Settings")
        windows_layout = QVBoxLayout(self.windows_group)

        self.windows_console_check = QCheckBox("Show console window")
        self.windows_console_check.stateChanged.connect(self._on_preset_modified)
        windows_layout.addWidget(self.windows_console_check)

        icon_layout = QHBoxLayout()
        icon_layout.addWidget(QLabel("Icon:"))
        self.windows_icon_edit = QLineEdit()
        self.windows_icon_edit.setPlaceholderText("Path to .ico file")
        self.windows_icon_edit.textChanged.connect(self._on_preset_modified)
        icon_layout.addWidget(self.windows_icon_edit)
        icon_browse_btn = QPushButton("Browse...")
        icon_browse_btn.clicked.connect(self._browse_icon)
        icon_layout.addWidget(icon_browse_btn)
        windows_layout.addLayout(icon_layout)

        layout.addWidget(self.windows_group)

        # macOS settings
        self.macos_group = QGroupBox("macOS Settings")
        macos_layout = QGridLayout(self.macos_group)

        macos_layout.addWidget(QLabel("Bundle Name:"), 0, 0)
        self.macos_bundle_name_edit = QLineEdit()
        self.macos_bundle_name_edit.setPlaceholderText("My Game")
        self.macos_bundle_name_edit.textChanged.connect(self._on_preset_modified)
        macos_layout.addWidget(self.macos_bundle_name_edit, 0, 1)

        macos_layout.addWidget(QLabel("Bundle ID:"), 1, 0)
        self.macos_bundle_id_edit = QLineEdit()
        self.macos_bundle_id_edit.setPlaceholderText("com.company.game")
        self.macos_bundle_id_edit.textChanged.connect(self._on_preset_modified)
        macos_layout.addWidget(self.macos_bundle_id_edit, 1, 1)

        macos_layout.addWidget(QLabel("Version:"), 2, 0)
        self.macos_version_edit = QLineEdit()
        self.macos_version_edit.setText("1.0.0")
        self.macos_version_edit.textChanged.connect(self._on_preset_modified)
        macos_layout.addWidget(self.macos_version_edit, 2, 1)

        self.macos_dmg_check = QCheckBox("Create DMG disk image")
        self.macos_dmg_check.stateChanged.connect(self._on_preset_modified)
        macos_layout.addWidget(self.macos_dmg_check, 3, 0, 1, 2)

        layout.addWidget(self.macos_group)

        # Web settings
        self.web_group = QGroupBox("Web Settings")
        web_layout = QVBoxLayout(self.web_group)

        resize_layout = QHBoxLayout()
        resize_layout.addWidget(QLabel("Canvas Resize:"))
        self.web_resize_combo = QComboBox()
        self.web_resize_combo.addItems(["Project Size", "Adaptive", "Fixed"])
        self.web_resize_combo.currentIndexChanged.connect(self._on_preset_modified)
        resize_layout.addWidget(self.web_resize_combo)
        resize_layout.addStretch()
        web_layout.addLayout(resize_layout)

        self.web_focus_check = QCheckBox("Auto-focus canvas on page load")
        self.web_focus_check.stateChanged.connect(self._on_preset_modified)
        web_layout.addWidget(self.web_focus_check)

        self.web_progressive_check = QCheckBox("Progressive loading (show progress bar)")
        self.web_progressive_check.stateChanged.connect(self._on_preset_modified)
        web_layout.addWidget(self.web_progressive_check)

        layout.addWidget(self.web_group)

        layout.addStretch()

    def _setup_resources_tab(self, layout: QVBoxLayout):
        """Setup resources/filtering tab"""
        # Exclude patterns
        exclude_group = QGroupBox("Exclude Patterns")
        exclude_layout = QVBoxLayout(exclude_group)

        exclude_layout.addWidget(QLabel("Files matching these patterns will be excluded:"))
        self.exclude_edit = QTextEdit()
        self.exclude_edit.setPlaceholderText("*.psd\n*.blend\n*.tmp\n__pycache__")
        self.exclude_edit.setMaximumHeight(100)
        self.exclude_edit.textChanged.connect(self._on_preset_modified)
        exclude_layout.addWidget(self.exclude_edit)

        layout.addWidget(exclude_group)

        # Include only
        include_group = QGroupBox("Include Only (leave empty for all)")
        include_layout = QVBoxLayout(include_group)

        include_layout.addWidget(QLabel("Only include files with these extensions:"))
        self.include_edit = QTextEdit()
        self.include_edit.setPlaceholderText(".png\n.jpg\n.json\n.scene\n.lua")
        self.include_edit.setMaximumHeight(100)
        self.include_edit.textChanged.connect(self._on_preset_modified)
        include_layout.addWidget(self.include_edit)

        layout.addWidget(include_group)

        # Debug options
        debug_group = QGroupBox("Debug Options")
        debug_layout = QVBoxLayout(debug_group)

        self.debug_symbols_check = QCheckBox("Include debug symbols")
        self.debug_symbols_check.setToolTip(
            "Export from the debug template instead of the release one.\n\n"
            "The debug template keeps symbols and function names, so crashes and "
            "profiles report real names instead of raw addresses. On Windows it also "
            "ships a .pdb and opens a console window; on web it keeps function names "
            "in the WebAssembly build and turns on runtime assertions.\n\n"
            "Debug builds are larger and slower - ship the release template instead."
        )
        self.debug_symbols_check.stateChanged.connect(self._on_preset_modified)
        debug_layout.addWidget(self.debug_symbols_check)

        self.editor_data_check = QCheckBox("Include editor-only data")
        self.editor_data_check.stateChanged.connect(self._on_preset_modified)
        debug_layout.addWidget(self.editor_data_check)

        layout.addWidget(debug_group)

        layout.addStretch()

    def _load_presets(self):
        """Load presets into the list"""
        self.presets_list.clear()

        # Add existing presets
        for name, preset in self.manager.presets.items():
            item = QListWidgetItem(name)
            item.setData(Qt.ItemDataRole.UserRole, preset)
            self.presets_list.addItem(item)

        # If no presets, create defaults
        if self.presets_list.count() == 0:
            for preset in create_default_presets():
                # Set default output path
                project_dir = os.path.dirname(self.project_path)
                preset.output_path = os.path.join(
                    project_dir, "export", preset.platform.platform_dir,
                    self.project_name + preset.platform.output_extension
                )
                preset.executable_name = self.project_name

                self.manager.add_preset(preset)

                item = QListWidgetItem(preset.name)
                item.setData(Qt.ItemDataRole.UserRole, preset)
                self.presets_list.addItem(item)

        # Select first item
        if self.presets_list.count() > 0:
            self.presets_list.setCurrentRow(0)

    def _on_preset_selected(self, current: QListWidgetItem, previous: QListWidgetItem):
        """Handle preset selection change"""
        if current is None:
            return

        preset = current.data(Qt.ItemDataRole.UserRole)
        if preset:
            self.current_preset = preset
            self._load_preset_to_ui(preset)
            self._update_template_status()

    def _load_preset_to_ui(self, preset: ExportPreset):
        """Load preset values into UI controls"""
        # Block signals during loading
        self._block_signals(True)

        self.preset_name_edit.setText(preset.name)

        # Find platform in combo
        for i in range(self.platform_combo.count()):
            if self.platform_combo.itemData(i) == preset.platform:
                self.platform_combo.setCurrentIndex(i)
                break

        self.exe_name_edit.setText(preset.executable_name)
        self.output_path_edit.setText(preset.output_path)

        # Graphics backend
        backend_map = {
            "opengl": 0, "vulkan": 1, "dx11": 2, "dx12": 3, "metal": 4, "webgl": 5
        }
        self.backend_combo.setCurrentIndex(backend_map.get(preset.graphics_backend, 0))

        self.embed_pack_check.setChecked(preset.embed_pack)
        self.compress_check.setChecked(preset.compress_assets)
        self.compress_level_spin.setValue(preset.compression_level)

        # Windows
        self.windows_console_check.setChecked(preset.windows_console)
        self.windows_icon_edit.setText(preset.windows_icon_path)

        # macOS
        self.macos_bundle_name_edit.setText(preset.macos_bundle_name)
        self.macos_bundle_id_edit.setText(preset.macos_bundle_id)
        self.macos_version_edit.setText(preset.macos_version)
        self.macos_dmg_check.setChecked(preset.macos_create_dmg)

        # Web
        resize_map = {"project": 0, "adaptive": 1, "fixed": 2}
        self.web_resize_combo.setCurrentIndex(resize_map.get(preset.web_canvas_resize, 0))
        self.web_focus_check.setChecked(preset.web_focus_canvas)
        self.web_progressive_check.setChecked(preset.web_progressive_loading)

        # Resources
        self.exclude_edit.setPlainText('\n'.join(preset.exclude_patterns))
        self.include_edit.setPlainText('\n'.join(preset.include_extensions))
        self.debug_symbols_check.setChecked(preset.include_debug_symbols)
        self.editor_data_check.setChecked(preset.include_editor_data)

        self._block_signals(False)
        self._update_platform_visibility()

    def _block_signals(self, block: bool):
        """Block or unblock UI signals"""
        widgets = [
            self.preset_name_edit, self.platform_combo, self.exe_name_edit,
            self.output_path_edit, self.backend_combo, self.embed_pack_check,
            self.compress_check, self.compress_level_spin, self.windows_console_check,
            self.windows_icon_edit, self.macos_bundle_name_edit, self.macos_bundle_id_edit,
            self.macos_version_edit, self.macos_dmg_check, self.web_resize_combo,
            self.web_focus_check, self.web_progressive_check, self.exclude_edit,
            self.include_edit, self.debug_symbols_check, self.editor_data_check
        ]
        for widget in widgets:
            widget.blockSignals(block)

    def _on_preset_modified(self):
        """Handle preset modification"""
        if not self.current_preset:
            return

        # Update preset from UI
        self.current_preset.name = self.preset_name_edit.text()
        self.current_preset.platform = self.platform_combo.currentData()
        self.current_preset.executable_name = self.exe_name_edit.text()
        self.current_preset.output_path = self.output_path_edit.text()

        backend_map = {0: "opengl", 1: "vulkan", 2: "dx11", 3: "dx12", 4: "metal", 5: "webgl"}
        self.current_preset.graphics_backend = backend_map.get(self.backend_combo.currentIndex(), "opengl")

        self.current_preset.embed_pack = self.embed_pack_check.isChecked()
        self.current_preset.compress_assets = self.compress_check.isChecked()
        self.current_preset.compression_level = self.compress_level_spin.value()

        # Windows
        self.current_preset.windows_console = self.windows_console_check.isChecked()
        self.current_preset.windows_icon_path = self.windows_icon_edit.text()

        # macOS
        self.current_preset.macos_bundle_name = self.macos_bundle_name_edit.text()
        self.current_preset.macos_bundle_id = self.macos_bundle_id_edit.text()
        self.current_preset.macos_version = self.macos_version_edit.text()
        self.current_preset.macos_create_dmg = self.macos_dmg_check.isChecked()

        # Web
        resize_map = {0: "project", 1: "adaptive", 2: "fixed"}
        self.current_preset.web_canvas_resize = resize_map.get(self.web_resize_combo.currentIndex(), "project")
        self.current_preset.web_focus_canvas = self.web_focus_check.isChecked()
        self.current_preset.web_progressive_loading = self.web_progressive_check.isChecked()

        # Resources
        self.current_preset.exclude_patterns = [
            p.strip() for p in self.exclude_edit.toPlainText().split('\n') if p.strip()
        ]
        self.current_preset.include_extensions = [
            p.strip() for p in self.include_edit.toPlainText().split('\n') if p.strip()
        ]
        self.current_preset.include_debug_symbols = self.debug_symbols_check.isChecked()
        self.current_preset.include_editor_data = self.editor_data_check.isChecked()

        # Update list item
        current_item = self.presets_list.currentItem()
        if current_item:
            current_item.setText(self.current_preset.name)
            current_item.setData(Qt.ItemDataRole.UserRole, self.current_preset)

        # Save presets
        self.manager.presets[self.current_preset.name] = self.current_preset
        self.manager.save_presets()

        # The debug-symbols toggle selects a different export template, so the
        # availability banner has to re-evaluate whenever the preset changes.
        self._update_template_status()

    def _on_platform_changed(self):
        """Handle platform selection change"""
        self._on_preset_modified()
        self._update_platform_visibility()
        self._update_template_status()

    def _update_platform_visibility(self):
        """Show/hide platform-specific settings"""
        if not self.current_preset:
            return

        platform = self.current_preset.platform

        self.windows_group.setVisible(platform == ExportPlatform.WINDOWS_X64)
        self.macos_group.setVisible(platform in [ExportPlatform.MACOS_ARM64, ExportPlatform.MACOS_X64])
        self.web_group.setVisible(platform == ExportPlatform.WEB)

        # Update embed pack option for web
        self.embed_pack_check.setEnabled(platform != ExportPlatform.WEB)
        if platform == ExportPlatform.WEB:
            self.embed_pack_check.setChecked(False)

    def _update_template_status(self):
        """Update template availability status"""
        if not self.current_preset:
            return

        platform = self.current_preset.platform
        want_debug = self.current_preset.include_debug_symbols
        available = self.manager.is_template_available(platform, debug=want_debug)
        fallback_available = self.manager.is_template_available(platform, debug=not want_debug)

        if available:
            variant = "Debug template" if want_debug else "Template"
            self.template_status.setText(f"{variant} available for {platform.display_name}")
            self.template_status.setStyleSheet(
                "background-color: #2d5a2d; color: white; padding: 5px; border-radius: 3px;"
            )
            self.export_btn.setEnabled(True)
        elif fallback_available:
            missing = "Debug template" if want_debug else "Release template"
            substitute = "release" if want_debug else "debug"
            self.template_status.setText(
                f"{missing} NOT found for {platform.display_name}. "
                f"Export will fall back to the {substitute} template."
            )
            self.template_status.setStyleSheet(
                "background-color: #5a4a2d; color: white; padding: 5px; border-radius: 3px;"
            )
            self.export_btn.setEnabled(True)
        else:
            self.template_status.setText(
                f"Template NOT found for {platform.display_name}. "
                "Build templates with: cmake -DLUPINE_BUILD_EXPORT_TEMPLATES=ON"
            )
            self.template_status.setStyleSheet(
                "background-color: #5a2d2d; color: white; padding: 5px; border-radius: 3px;"
            )
            self.export_btn.setEnabled(False)

    def _browse_output(self):
        """Browse for output path"""
        if not self.current_preset:
            return

        platform = self.current_preset.platform
        filter_str = ""
        if platform == ExportPlatform.WINDOWS_X64:
            filter_str = "Executable (*.exe)"
        elif platform == ExportPlatform.WEB:
            filter_str = "HTML File (*.html)"
        else:
            filter_str = "All Files (*)"

        path, _ = QFileDialog.getSaveFileName(
            self, "Select Output Path",
            self.output_path_edit.text(),
            filter_str
        )

        if path:
            self.output_path_edit.setText(path)

    def _browse_icon(self):
        """Browse for Windows icon"""
        path, _ = QFileDialog.getOpenFileName(
            self, "Select Icon",
            "",
            "Icon Files (*.ico)"
        )
        if path:
            self.windows_icon_edit.setText(path)

    def _add_preset(self):
        """Add a new preset"""
        preset = ExportPreset(
            name="New Preset",
            platform=get_current_platform()
        )

        project_dir = os.path.dirname(self.project_path)
        preset.output_path = os.path.join(
            project_dir, "export", preset.platform.platform_dir,
            self.project_name + preset.platform.output_extension
        )
        preset.executable_name = self.project_name

        self.manager.add_preset(preset)

        item = QListWidgetItem(preset.name)
        item.setData(Qt.ItemDataRole.UserRole, preset)
        self.presets_list.addItem(item)
        self.presets_list.setCurrentItem(item)

    def _remove_preset(self):
        """Remove the selected preset"""
        current = self.presets_list.currentItem()
        if current:
            name = current.text()
            self.manager.remove_preset(name)
            self.presets_list.takeItem(self.presets_list.row(current))

    def _duplicate_preset(self):
        """Duplicate the selected preset"""
        if not self.current_preset:
            return

        new_preset = ExportPreset.from_dict(self.current_preset.to_dict())
        new_preset.name = f"{self.current_preset.name} (Copy)"

        self.manager.add_preset(new_preset)

        item = QListWidgetItem(new_preset.name)
        item.setData(Qt.ItemDataRole.UserRole, new_preset)
        self.presets_list.addItem(item)
        self.presets_list.setCurrentItem(item)

    def _start_export(self):
        """Start the export process"""
        if not self.current_preset:
            return

        if not self.current_preset.output_path:
            QMessageBox.warning(self, "Export Error", "Please specify an output path.")
            return

        self.export_btn.setEnabled(False)
        self.progress_bar.setVisible(True)
        self.progress_bar.setValue(0)
        self.log_output.setVisible(True)
        self.log_output.clear()

        self.worker = ExportWorker(self.manager, self.project_path, self.current_preset)
        self.worker.progress.connect(self._on_export_progress)
        self.worker.finished.connect(self._on_export_finished)
        self.worker.start()

    def _on_export_progress(self, message: str, progress: float):
        """Handle export progress updates"""
        self.progress_label.setText(message)
        self.log_output.append(message)

        if progress >= 0:
            self.progress_bar.setValue(int(progress * 100))

    def _on_export_finished(self, result: ExportResult):
        """Handle export completion"""
        self.export_btn.setEnabled(True)
        self.progress_bar.setVisible(False)
        self.worker = None

        if result.success:
            self.progress_label.setText(f"Export complete! ({result.files_packed} files)")
            self.log_output.append(f"\nExport successful!")
            self.log_output.append(f"Output: {result.output_path}")
            self.log_output.append(f"Files packed: {result.files_packed}")
            self.log_output.append(f"Total size: {result.total_size / 1024:.1f} KB")
            self.log_output.append(f"Time: {result.export_time:.2f}s")

            QMessageBox.information(
                self, "Export Complete",
                f"Project exported successfully!\n\n"
                f"Output: {result.output_path}\n"
                f"Files: {result.files_packed}"
            )
        else:
            self.progress_label.setText("Export failed!")
            for error in result.errors:
                self.log_output.append(f"ERROR: {error}")

            QMessageBox.critical(
                self, "Export Failed",
                "Export failed:\n\n" + "\n".join(result.errors)
            )

    def _export_all(self):
        """Export all presets"""
        QMessageBox.information(
            self, "Export All",
            "Export all presets feature coming soon!\n\n"
            "This will export the project using all configured presets."
        )

    def closeEvent(self, event):
        """Handle dialog close"""
        if self.worker and self.worker.isRunning():
            QMessageBox.warning(
                self, "Export in Progress",
                "Please wait for the current export to complete."
            )
            event.ignore()
            return

        self.manager.save_presets()
        event.accept()
