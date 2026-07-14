"""
Godot Scene Import Dialog
Wizard for importing Godot .tscn scenes into Lupine Engine
"""

from PyQt6.QtWidgets import (QDialog, QVBoxLayout, QHBoxLayout, QLabel, QPushButton,
                             QFileDialog, QTreeWidget, QTreeWidgetItem, QCheckBox,
                             QComboBox, QGroupBox, QScrollArea, QWidget, QLineEdit,
                             QTabWidget, QSplitter, QProgressBar, QMessageBox,
                             QHeaderView, QStyle, QFrame, QSizePolicy)
from PyQt6.QtCore import Qt, pyqtSignal, QSize
from PyQt6.QtGui import QIcon, QColor, QPixmap, QPainter, QFont
from pathlib import Path
import lupine_engine as le
from editor.theme import get_theme_manager


class AssetTreeItem(QTreeWidgetItem):
    """Tree item for an asset in the import list"""

    def __init__(self, asset_data, parent=None):
        super().__init__(parent)
        self.asset_data = asset_data
        self._setup_ui()

    def _setup_ui(self):
        asset = self.asset_data

        # Column 0: Checkbox + Name
        filename = asset.godot_path.split('/')[-1]
        self.setText(0, filename)
        self.setCheckState(0, Qt.CheckState.Checked if asset.is_selected else Qt.CheckState.Unchecked)
        self.setFlags(self.flags() | Qt.ItemFlag.ItemIsUserCheckable)

        # Column 1: Type
        type_names = {
            le.ImportAssetType.Texture: "Texture",
            le.ImportAssetType.Audio: "Audio",
            le.ImportAssetType.Model: "Model",
            le.ImportAssetType.Font: "Font",
            le.ImportAssetType.Script: "Script",
            le.ImportAssetType.Scene: "Scene",
            le.ImportAssetType.Resource: "Resource",
            le.ImportAssetType.Unknown: "Unknown"
        }
        self.setText(1, type_names.get(asset.type, "Unknown"))

        # Column 2: Status
        status_names = {
            le.ImportAssetStatus.Resolved: "Ready",
            le.ImportAssetStatus.Missing: "Missing",
            le.ImportAssetStatus.NeedsMapping: "Needs Mapping",
            le.ImportAssetStatus.Script: "Script"
        }
        status_text = status_names.get(asset.status, "Unknown")
        self.setText(2, status_text)

        # Column 3: Path
        self.setText(3, asset.godot_path)

        # Set colors based on status
        theme = get_theme_manager().get_current_theme()
        if asset.status == le.ImportAssetStatus.Missing:
            self.setForeground(2, QColor("#ff6b6b"))  # Red for missing
        elif asset.status == le.ImportAssetStatus.Script:
            self.setForeground(2, QColor("#ffd93d"))  # Yellow for scripts
        elif asset.status == le.ImportAssetStatus.Resolved:
            self.setForeground(2, QColor("#6bcb77"))  # Green for resolved

        # Tooltip
        self.setToolTip(0, f"Original: {asset.godot_path}\nTarget: {asset.lupine_path}")


class NodeTreeItem(QTreeWidgetItem):
    """Tree item for a node in the scene preview"""

    def __init__(self, name, godot_type, lupine_type, is_mapped, parent=None):
        super().__init__(parent)
        self.godot_type = godot_type
        self.lupine_type = lupine_type
        self.is_mapped = is_mapped

        self.setText(0, name)
        self.setText(1, godot_type if godot_type else "(instance)")
        self.setText(2, lupine_type if lupine_type else "-")

        # Color based on mapping status
        if not is_mapped and godot_type:
            self.setForeground(1, QColor("#ffd93d"))  # Yellow for unmapped
            self.setForeground(2, QColor("#ffd93d"))


class ScriptLanguageWidget(QWidget):
    """Widget for selecting script language for blank script creation"""

    language_changed = pyqtSignal(str)

    def __init__(self, script_name, parent=None):
        super().__init__(parent)
        self.script_name = script_name

        layout = QHBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)

        self.checkbox = QCheckBox("Create blank:")
        self.checkbox.setChecked(True)
        self.checkbox.stateChanged.connect(self._on_checkbox_changed)
        layout.addWidget(self.checkbox)

        self.combo = QComboBox()
        self.combo.addItems(["Lua", "Python", "mruby"])
        self.combo.currentTextChanged.connect(lambda t: self.language_changed.emit(t))
        layout.addWidget(self.combo)

        layout.addStretch()

    def _on_checkbox_changed(self, state):
        self.combo.setEnabled(state == Qt.CheckState.Checked.value)

    def is_create_blank(self):
        return self.checkbox.isChecked()

    def get_language(self):
        return self.combo.currentText()


class GodotSceneImportDialog(QDialog):
    """Main dialog for importing Godot scenes"""

    import_completed = pyqtSignal(str)  # Emits the path to the imported scene

    def __init__(self, editor_bridge, project_path, parent=None):
        super().__init__(parent)
        self.editor_bridge = editor_bridge
        self.project_path = Path(project_path)
        self.godot_file_path = None
        self.import_result = None
        self.script_widgets = {}  # Map script path to ScriptLanguageWidget

        self.setWindowTitle("Import Godot Scene")
        self.setMinimumSize(900, 700)
        self.setModal(True)

        self._setup_ui()

    def _setup_ui(self):
        """Setup the dialog UI"""
        layout = QVBoxLayout(self)

        # File selection area
        file_group = QGroupBox("Source File")
        file_layout = QHBoxLayout(file_group)

        self.file_path_edit = QLineEdit()
        self.file_path_edit.setPlaceholderText("Select a Godot scene file (.tscn)...")
        self.file_path_edit.setReadOnly(True)
        file_layout.addWidget(self.file_path_edit)

        browse_btn = QPushButton("Browse...")
        browse_btn.clicked.connect(self._browse_file)
        file_layout.addWidget(browse_btn)

        layout.addWidget(file_group)

        # Main content area with tabs
        self.tabs = QTabWidget()

        # Tab 1: Scene Preview
        scene_tab = QWidget()
        scene_layout = QVBoxLayout(scene_tab)

        # Scene tree
        self.scene_tree = QTreeWidget()
        self.scene_tree.setHeaderLabels(["Node", "Godot Type", "Lupine Type"])
        self.scene_tree.header().setSectionResizeMode(QHeaderView.ResizeMode.ResizeToContents)
        scene_layout.addWidget(self.scene_tree)

        # Unmapped nodes info
        self.unmapped_label = QLabel()
        self.unmapped_label.setWordWrap(True)
        scene_layout.addWidget(self.unmapped_label)

        self.tabs.addTab(scene_tab, "Scene Structure")

        # Tab 2: Assets
        assets_tab = QWidget()
        assets_layout = QVBoxLayout(assets_tab)

        # Assets tree
        self.assets_tree = QTreeWidget()
        self.assets_tree.setHeaderLabels(["Asset", "Type", "Status", "Path"])
        self.assets_tree.header().setSectionResizeMode(QHeaderView.ResizeMode.ResizeToContents)
        self.assets_tree.itemChanged.connect(self._on_asset_item_changed)
        assets_layout.addWidget(self.assets_tree)

        # Asset summary
        self.asset_summary_label = QLabel()
        assets_layout.addWidget(self.asset_summary_label)

        self.tabs.addTab(assets_tab, "Assets")

        # Tab 3: Scripts
        scripts_tab = QWidget()
        scripts_layout = QVBoxLayout(scripts_tab)

        scripts_info = QLabel(
            "<b>⚠ GDScript is NOT translated.</b> Lupine cannot convert GDScript logic, "
            "so each script is imported as an <b>empty stub</b>: the class and its method "
            "names are recreated, but every method body is left blank for you to "
            "reimplement. The original method names are preserved so the scaffolding is "
            "ready if a GDScript translator is added later."
        )
        scripts_info.setWordWrap(True)
        scripts_info.setTextFormat(Qt.TextFormat.RichText)
        scripts_info.setStyleSheet(
            "QLabel { background-color: #4a3a1a; color: #ffd93d; border: 1px solid #ffd93d; "
            "border-radius: 4px; padding: 8px; }"
        )
        scripts_layout.addWidget(scripts_info)

        # Scripts list with language selection
        self.scripts_area = QScrollArea()
        self.scripts_area.setWidgetResizable(True)
        self.scripts_content = QWidget()
        self.scripts_layout = QVBoxLayout(self.scripts_content)
        self.scripts_layout.setAlignment(Qt.AlignmentFlag.AlignTop)
        self.scripts_area.setWidget(self.scripts_content)
        scripts_layout.addWidget(self.scripts_area)

        # Default language selection
        default_lang_layout = QHBoxLayout()
        default_lang_layout.addWidget(QLabel("Default language for all scripts:"))
        self.default_language_combo = QComboBox()
        self.default_language_combo.addItems(["Lua", "Python", "mruby"])
        self.default_language_combo.currentTextChanged.connect(self._on_default_language_changed)
        default_lang_layout.addWidget(self.default_language_combo)
        default_lang_layout.addStretch()
        scripts_layout.addLayout(default_lang_layout)

        self.tabs.addTab(scripts_tab, "Scripts")

        # Tab 4: Missing Assets
        missing_tab = QWidget()
        missing_layout = QVBoxLayout(missing_tab)

        missing_info = QLabel(
            "Assets that couldn't be found can be manually mapped to existing "
            "project files or left as missing (you can fix them after import)."
        )
        missing_info.setWordWrap(True)
        missing_layout.addWidget(missing_info)

        self.missing_tree = QTreeWidget()
        self.missing_tree.setHeaderLabels(["Original Path", "Mapped To", "Action"])
        self.missing_tree.header().setSectionResizeMode(QHeaderView.ResizeMode.ResizeToContents)
        missing_layout.addWidget(self.missing_tree)

        self.tabs.addTab(missing_tab, "Missing Assets")

        layout.addWidget(self.tabs)

        # Import options
        options_group = QGroupBox("Import Options")
        options_layout = QVBoxLayout(options_group)

        options_row1 = QHBoxLayout()
        self.copy_assets_check = QCheckBox("Copy resolved assets to project")
        self.copy_assets_check.setChecked(True)
        options_row1.addWidget(self.copy_assets_check)

        self.create_blanks_check = QCheckBox("Create stub scripts for GDScript files")
        self.create_blanks_check.setChecked(True)
        self.create_blanks_check.setToolTip(
            "GDScript logic is not translated. When enabled, each GDScript file is "
            "imported as an empty stub with the original class and method names but no "
            "method bodies, ready for you to reimplement."
        )
        options_row1.addWidget(self.create_blanks_check)
        options_row1.addStretch()
        options_layout.addLayout(options_row1)

        # Target directory
        target_layout = QHBoxLayout()
        target_layout.addWidget(QLabel("Import to:"))
        self.target_edit = QLineEdit()
        self.target_edit.setText("res://imported/")
        target_layout.addWidget(self.target_edit)
        options_layout.addLayout(target_layout)

        layout.addWidget(options_group)

        # Progress bar
        self.progress_bar = QProgressBar()
        self.progress_bar.setVisible(False)
        layout.addWidget(self.progress_bar)

        # Status label
        self.status_label = QLabel("Select a Godot scene file to begin.")
        layout.addWidget(self.status_label)

        # Buttons
        button_layout = QHBoxLayout()
        button_layout.addStretch()

        self.analyze_btn = QPushButton("Analyze")
        self.analyze_btn.clicked.connect(self._analyze_scene)
        self.analyze_btn.setEnabled(False)
        button_layout.addWidget(self.analyze_btn)

        self.import_btn = QPushButton("Import")
        self.import_btn.clicked.connect(self._import_scene)
        self.import_btn.setEnabled(False)
        button_layout.addWidget(self.import_btn)

        cancel_btn = QPushButton("Cancel")
        cancel_btn.clicked.connect(self.reject)
        button_layout.addWidget(cancel_btn)

        layout.addLayout(button_layout)

    def _browse_file(self):
        """Open file browser to select a Godot scene"""
        file_path, _ = QFileDialog.getOpenFileName(
            self,
            "Select Godot Scene",
            str(self.project_path),
            "Godot Scene Files (*.tscn);;All Files (*)"
        )

        if file_path:
            self.godot_file_path = file_path
            self.file_path_edit.setText(file_path)
            self.analyze_btn.setEnabled(True)
            self.status_label.setText("Click 'Analyze' to preview the import.")

            # Auto-analyze
            self._analyze_scene()

    def _analyze_scene(self):
        """Analyze the selected Godot scene"""
        if not self.godot_file_path:
            return

        self.status_label.setText("Analyzing scene...")
        self.progress_bar.setVisible(True)
        self.progress_bar.setRange(0, 0)  # Indeterminate

        try:
            # Create importer and analyze
            self.importer = le.GodotSceneImporter()

            # Set config
            config = le.ImportConfig()
            config.target_directory = self.target_edit.text()
            config.copy_assets = self.copy_assets_check.isChecked()
            config.create_blank_scripts = self.create_blanks_check.isChecked()
            config.default_script_language = self.default_language_combo.currentText()
            config.lupine_project_root = str(self.project_path)

            # Try to guess Godot project root
            godot_path = Path(self.godot_file_path)
            for parent in godot_path.parents:
                if (parent / "project.godot").exists():
                    config.godot_project_root = str(parent)
                    break

            self.importer.set_config(config)

            # Parse the scene first to get tree structure
            parser = le.GodotSceneParser()
            parsed = parser.parse_file(self.godot_file_path)
            if parsed:
                parsed.build_node_hierarchy()
                self.parsed_scene = parsed
            else:
                self.parsed_scene = None

            # Analyze
            self.import_result = self.importer.analyze(self.godot_file_path)

            self._populate_scene_tree()
            self._populate_assets_tree()
            self._populate_scripts_list()
            self._populate_missing_assets()
            self._update_summary()

            self.import_btn.setEnabled(True)
            self.status_label.setText("Analysis complete. Review and click 'Import' to proceed.")

        except Exception as e:
            self.status_label.setText(f"Error analyzing scene: {str(e)}")
            QMessageBox.critical(self, "Analysis Error", f"Failed to analyze scene:\n{str(e)}")

        finally:
            self.progress_bar.setVisible(False)

    def _populate_scene_tree(self):
        """Populate the scene tree with node hierarchy"""
        self.scene_tree.clear()

        if not self.import_result:
            return

        # If we have parsed scene data, build the actual tree
        if hasattr(self, 'parsed_scene') and self.parsed_scene and self.parsed_scene.root_node:
            self._build_node_tree_item(self.parsed_scene.root_node, self.scene_tree)
        else:
            # Fallback to summary view
            root_item = QTreeWidgetItem(self.scene_tree)
            root_item.setText(0, "Imported Scene")
            root_item.setText(1, "-")
            root_item.setText(2, "Scene")

            summary_item = QTreeWidgetItem(root_item)
            summary_item.setText(0, f"{self.import_result.total_nodes} nodes")
            summary_item.setText(1, f"{self.import_result.converted_nodes} converted")
            summary_item.setText(2, f"{self.import_result.skipped_nodes} unsupported")

        # Show unmapped nodes
        unmapped_text = ""
        if self.import_result.unmapped_nodes:
            unmapped_types = set()
            for node in self.import_result.unmapped_nodes:
                unmapped_types.add(node.godot_type)
            unmapped_text = f"Unsupported node types: {', '.join(sorted(unmapped_types))}"
        else:
            unmapped_text = "All node types are supported!"

        self.unmapped_label.setText(unmapped_text)
        self.scene_tree.expandAll()

    def _build_node_tree_item(self, godot_node, parent):
        """Recursively build tree items for a Godot node and its children"""
        if parent is None:
            return

        # Create tree item
        if isinstance(parent, QTreeWidget):
            item = QTreeWidgetItem(parent)
        else:
            item = QTreeWidgetItem(parent)

        # Column 0: Node name
        item.setText(0, godot_node.name)

        # Column 1: Godot type
        godot_type = godot_node.type if godot_node.type else "(instance)"
        item.setText(1, godot_type)

        # Column 2: Lupine mapping
        if godot_node.type and hasattr(self, 'importer'):
            mapping = self.importer.get_mapping(godot_node.type)
            if mapping:
                lupine_type = mapping.lupine_node_type
                components = mapping.components
                if components:
                    mapping_text = f"{lupine_type} + {', '.join(components)}"
                else:
                    mapping_text = lupine_type
                item.setText(2, mapping_text)
                item.setForeground(2, QColor("#4CAF50"))  # Green for supported
            else:
                item.setText(2, "Not supported")
                item.setForeground(2, QColor("#F44336"))  # Red for unsupported
        elif godot_node.instance:
            item.setText(2, "Scene instance")
            item.setForeground(2, QColor("#2196F3"))  # Blue for instances
        else:
            item.setText(2, "-")

        # Recursively add children
        for child in godot_node.children:
            self._build_node_tree_item(child, item)

    def _populate_assets_tree(self):
        """Populate the assets tree"""
        self.assets_tree.clear()

        if not self.import_result:
            return

        # Group assets by type
        type_groups = {}
        for asset in self.import_result.assets:
            type_name = str(asset.type).split('.')[-1]
            if type_name not in type_groups:
                type_groups[type_name] = []
            type_groups[type_name].append(asset)

        for type_name, assets in type_groups.items():
            # Skip scripts - they have their own tab
            if type_name == "Script":
                continue

            group_item = QTreeWidgetItem(self.assets_tree)
            group_item.setText(0, f"{type_name} ({len(assets)})")
            group_item.setFlags(group_item.flags() & ~Qt.ItemFlag.ItemIsUserCheckable)

            for asset in assets:
                AssetTreeItem(asset, group_item)

        self.assets_tree.expandAll()

    def _populate_scripts_list(self):
        """Populate the scripts list with language selection"""
        # Clear existing
        while self.scripts_layout.count():
            item = self.scripts_layout.takeAt(0)
            if item.widget():
                item.widget().deleteLater()

        self.script_widgets.clear()

        if not self.import_result:
            return

        # Find script assets
        scripts = [a for a in self.import_result.assets
                   if a.status == le.ImportAssetStatus.Script]

        if not scripts:
            no_scripts = QLabel("No GDScript files found in this scene.")
            no_scripts.setStyleSheet("color: gray;")
            self.scripts_layout.addWidget(no_scripts)
            return

        for script in scripts:
            script_name = script.godot_path.split('/')[-1]

            # Script info frame
            frame = QFrame()
            frame.setFrameShape(QFrame.Shape.StyledPanel)
            frame_layout = QVBoxLayout(frame)

            # Script name
            name_label = QLabel(f"<b>{script_name}</b>")
            frame_layout.addWidget(name_label)

            # Original path
            path_label = QLabel(f"Original: {script.godot_path}")
            path_label.setStyleSheet("color: gray; font-size: 10px;")
            frame_layout.addWidget(path_label)

            # Stub reminder
            stub_label = QLabel("Imported as an empty stub — GDScript logic is not translated.")
            stub_label.setStyleSheet("color: #ffd93d; font-size: 10px; font-style: italic;")
            stub_label.setWordWrap(True)
            frame_layout.addWidget(stub_label)

            # Language widget
            lang_widget = ScriptLanguageWidget(script_name)
            self.script_widgets[script.godot_path] = lang_widget
            frame_layout.addWidget(lang_widget)

            self.scripts_layout.addWidget(frame)

        self.scripts_layout.addStretch()

    def _populate_missing_assets(self):
        """Populate the missing assets list"""
        self.missing_tree.clear()

        if not self.import_result:
            return

        missing = [a for a in self.import_result.assets
                   if a.status == le.ImportAssetStatus.Missing]

        for asset in missing:
            item = QTreeWidgetItem(self.missing_tree)
            item.setText(0, asset.godot_path)
            item.setText(1, "(not mapped)")
            item.setText(2, "Skip")
            item.setData(0, Qt.ItemDataRole.UserRole, asset)

            # Add browse button would go here in a more complex implementation

    def _update_summary(self):
        """Update the asset summary label"""
        if not self.import_result:
            self.asset_summary_label.setText("")
            return

        resolved = self.import_result.resolved_assets
        missing = self.import_result.missing_assets
        total = self.import_result.total_assets
        scripts = len([a for a in self.import_result.assets
                       if a.status == le.ImportAssetStatus.Script])

        summary = f"Assets: {resolved} resolved, {missing} missing, {scripts} scripts"
        if self.import_result.warnings:
            summary += f" | {len(self.import_result.warnings)} warnings"

        self.asset_summary_label.setText(summary)

    def _on_asset_item_changed(self, item, column):
        """Handle asset checkbox changes"""
        if column == 0 and hasattr(item, 'asset_data'):
            item.asset_data.is_selected = (item.checkState(0) == Qt.CheckState.Checked)

    def _on_default_language_changed(self, language):
        """Apply default language to all script widgets"""
        for widget in self.script_widgets.values():
            widget.combo.setCurrentText(language)

    def _import_scene(self):
        """Perform the actual import"""
        if not self.godot_file_path or not self.import_result:
            return

        self.status_label.setText("Importing scene...")
        self.progress_bar.setVisible(True)
        self.progress_bar.setRange(0, 100)
        self.progress_bar.setValue(0)

        try:
            # Create importer with updated config
            importer = le.GodotSceneImporter()

            config = le.ImportConfig()
            config.target_directory = self.target_edit.text()
            config.copy_assets = self.copy_assets_check.isChecked()
            config.create_blank_scripts = self.create_blanks_check.isChecked()
            config.default_script_language = self.default_language_combo.currentText()
            config.lupine_project_root = str(self.project_path)

            # Try to find Godot project root
            godot_path = Path(self.godot_file_path)
            for parent in godot_path.parents:
                if (parent / "project.godot").exists():
                    config.godot_project_root = str(parent)
                    break

            importer.set_config(config)

            self.progress_bar.setValue(10)

            # Update asset selections from UI
            for asset in self.import_result.assets:
                if asset.status == le.ImportAssetStatus.Script:
                    widget = self.script_widgets.get(asset.godot_path)
                    if widget:
                        asset.create_blank_script = widget.is_create_blank()
                        asset.blank_script_language = widget.get_language()

            self.progress_bar.setValue(20)

            # Perform import
            result = importer.do_import(self.godot_file_path)

            self.progress_bar.setValue(50)

            if not result.success:
                error_msg = "\n".join(result.errors) if result.errors else "Unknown error"
                raise Exception(error_msg)

            # Copy assets
            if config.copy_assets:
                copied = importer.copy_assets(result.assets)
                self.progress_bar.setValue(70)

            # Create blank scripts
            if config.create_blank_scripts:
                created = importer.create_blank_scripts(result.assets, config.default_script_language)
                self.progress_bar.setValue(85)

            # Save the scene
            if result.scene:
                target_dir = self.target_edit.text()
                if target_dir.startswith("res://"):
                    target_dir = target_dir[6:]

                scene_path = self.project_path / target_dir
                scene_path.mkdir(parents=True, exist_ok=True)

                scene_name = Path(self.godot_file_path).stem + ".scene"
                full_path = scene_path / scene_name

                result.scene.save(str(full_path))
                self.progress_bar.setValue(90)

                # Handle referenced (instanced) scenes - import them recursively
                referenced_scenes_imported = []
                if hasattr(result, 'referenced_scenes') and result.referenced_scenes:
                    godot_root = Path(config.godot_project_root) if config.godot_project_root else None

                    for ref_scene_path in result.referenced_scenes:
                        try:
                            # Convert Godot res:// path to actual file path
                            if ref_scene_path.startswith("res://"):
                                rel_path = ref_scene_path[6:]
                            else:
                                rel_path = ref_scene_path

                            if godot_root:
                                abs_ref_path = godot_root / rel_path
                                if abs_ref_path.exists():
                                    # Import the referenced scene
                                    ref_result = importer.do_import(str(abs_ref_path))
                                    if ref_result.success and ref_result.scene:
                                        # Save with same name structure
                                        ref_scene_name = Path(rel_path).stem + ".scene"
                                        ref_full_path = scene_path / ref_scene_name
                                        ref_result.scene.save(str(ref_full_path))
                                        referenced_scenes_imported.append(ref_scene_name)
                        except Exception as ref_e:
                            result.warnings.append(f"Failed to import referenced scene {ref_scene_path}: {str(ref_e)}")

                self.progress_bar.setValue(100)

                # Show success message
                warnings_text = ""
                if result.warnings:
                    warnings_text = f"\n\nWarnings ({len(result.warnings)}):\n" + "\n".join(result.warnings[:10])
                    if len(result.warnings) > 10:
                        warnings_text += f"\n... and {len(result.warnings) - 10} more"

                referenced_text = ""
                if referenced_scenes_imported:
                    referenced_text = f"\n\nReferenced scenes imported ({len(referenced_scenes_imported)}):\n" + "\n".join(referenced_scenes_imported[:5])
                    if len(referenced_scenes_imported) > 5:
                        referenced_text += f"\n... and {len(referenced_scenes_imported) - 5} more"

                # Make the script-stub limitation unmissable in the summary
                script_stub_text = ""
                num_stubbed = sum(1 for w in self.script_widgets.values() if w.is_create_blank())
                if config.create_blank_scripts and num_stubbed:
                    script_stub_text = (
                        f"\n\n⚠ {num_stubbed} GDScript file(s) were imported as EMPTY STUBS.\n"
                        f"GDScript logic is not translated — only the class and method names "
                        f"were recreated. You must reimplement each method body yourself."
                    )

                QMessageBox.information(
                    self,
                    "Import Complete",
                    f"Scene imported successfully!\n\n"
                    f"Nodes: {result.converted_nodes}/{result.total_nodes} converted\n"
                    f"Assets: {result.resolved_assets}/{result.total_assets} resolved\n"
                    f"Saved to: {full_path}"
                    f"{referenced_text}"
                    f"{script_stub_text}"
                    f"{warnings_text}"
                )

                self.import_completed.emit(str(full_path))
                self.accept()

        except Exception as e:
            self.status_label.setText(f"Import failed: {str(e)}")
            QMessageBox.critical(self, "Import Error", f"Failed to import scene:\n{str(e)}")

        finally:
            self.progress_bar.setVisible(False)

    @staticmethod
    def import_godot_scene(editor_bridge, project_path, parent=None):
        """
        Static helper method to show the import dialog and return the result.

        Returns the path to the imported scene if successful, None otherwise.
        """
        dialog = GodotSceneImportDialog(editor_bridge, project_path, parent)
        result_path = None

        def on_import_completed(path):
            nonlocal result_path
            result_path = path

        dialog.import_completed.connect(on_import_completed)

        if dialog.exec() == QDialog.DialogCode.Accepted:
            return result_path
        return None
