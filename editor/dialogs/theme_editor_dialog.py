"""
Theme Editor Dialog
Allows editing and customization of editor themes
"""

from PyQt6.QtWidgets import (QDialog, QVBoxLayout, QHBoxLayout, QPushButton,
                             QWidget, QLabel, QLineEdit, QSpinBox, QScrollArea,
                             QGroupBox, QFormLayout, QMessageBox, QFileDialog,
                             QColorDialog, QTabWidget, QComboBox)
from PyQt6.QtCore import Qt, pyqtSignal
from PyQt6.QtGui import QColor, QPalette
from pathlib import Path
import sys
import json
import copy

# Add parent directory to path to import theme
sys.path.insert(0, str(Path(__file__).parent.parent))
from theme import Theme, ThemeColors, get_theme_manager, get_icon_pack_manager


class ColorButton(QPushButton):
    """Button that displays and allows selecting a color"""
    color_changed = pyqtSignal(str)  # Emits color as hex string
    
    def __init__(self, color="#000000", parent=None):
        super().__init__(parent)
        self._color = color
        self.setFixedSize(80, 28)
        self.clicked.connect(self._pick_color)
        self._update_style()
    
    def _update_style(self):
        """Update button appearance to show the color"""
        self.setText(self._color)
        self.setStyleSheet(f"""
            QPushButton {{
                background-color: {self._color};
                color: {"#ffffff" if self._is_dark(self._color) else "#000000"};
                border: 2px solid #666;
                border-radius: 3px;
                font-family: monospace;
                font-size: 11px;
            }}
            QPushButton:hover {{
                border-color: #999;
            }}
        """)
    
    def _is_dark(self, color_hex):
        """Check if a color is dark (for text contrast)"""
        try:
            color = QColor(color_hex)
            # Calculate luminance
            r, g, b = color.red(), color.green(), color.blue()
            luminance = (0.299 * r + 0.587 * g + 0.114 * b)
            return luminance < 128
        except:
            return True
    
    def _pick_color(self):
        """Open color picker dialog"""
        color = QColorDialog.getColor(QColor(self._color), self, "Select Color")
        if color.isValid():
            self.set_color(color.name())
            self.color_changed.emit(self._color)
    
    def set_color(self, color_hex):
        """Set the button color"""
        self._color = color_hex
        self._update_style()
    
    def get_color(self):
        """Get the current color as hex string"""
        return self._color


class ThemeEditorDialog(QDialog):
    """Dialog for editing theme settings"""
    
    theme_changed = pyqtSignal(Theme)  # Emits when theme is applied
    
    def __init__(self, parent=None):
        super().__init__(parent)
        self.theme_manager = get_theme_manager()
        self.current_theme = copy.deepcopy(self.theme_manager.get_current_theme())
        self.original_theme = self.theme_manager.get_current_theme()
        self.color_buttons = {}
        self.spacing_widgets = {}
        
        self.setWindowTitle("Theme Editor")
        self.setModal(False)  # Allow interaction with main window for preview
        self.setMinimumSize(700, 750)
        
        self._setup_ui()
        self._load_theme_values()
    
    def _setup_ui(self):
        """Setup the dialog UI"""
        layout = QVBoxLayout()
        layout.setSpacing(10)
        
        # Title and theme selector
        header_layout = QHBoxLayout()
        title_label = QLabel("Theme Editor")
        title_label.setStyleSheet("font-size: 18px; font-weight: bold;")
        header_layout.addWidget(title_label)
        
        header_layout.addStretch()
        
        # Theme selector
        header_layout.addWidget(QLabel("Base Theme:"))
        self.theme_combo = QComboBox()
        self.theme_combo.addItems(self.theme_manager.get_theme_names())
        self.theme_combo.setCurrentText(self.current_theme.name)
        self.theme_combo.currentTextChanged.connect(self._on_theme_changed)
        header_layout.addWidget(self.theme_combo)
        
        layout.addLayout(header_layout)
        
        # Theme name editor
        name_layout = QHBoxLayout()
        name_layout.addWidget(QLabel("Theme Name:"))
        self.theme_name_edit = QLineEdit()
        self.theme_name_edit.setText(self.current_theme.name)
        self.theme_name_edit.textChanged.connect(self._on_name_changed)
        name_layout.addWidget(self.theme_name_edit)
        layout.addLayout(name_layout)
        
        # Tabs for different color categories
        tabs = QTabWidget()
        tabs.addTab(self._create_main_colors_tab(), "Main Colors")
        tabs.addTab(self._create_accent_colors_tab(), "Accent Colors")
        tabs.addTab(self._create_status_colors_tab(), "Status Colors")
        tabs.addTab(self._create_ui_colors_tab(), "UI Elements")
        tabs.addTab(self._create_text_colors_tab(), "Text Colors")
        tabs.addTab(self._create_special_colors_tab(), "Special")
        tabs.addTab(self._create_spacing_tab(), "Spacing & Icons")
        
        layout.addWidget(tabs)
        
        # Action buttons
        button_layout = QHBoxLayout()
        
        # Left side buttons
        import_btn = QPushButton("Import Theme...")
        import_btn.setProperty("secondary", True)
        import_btn.clicked.connect(self._import_theme)
        button_layout.addWidget(import_btn)
        
        export_btn = QPushButton("Export Theme...")
        export_btn.setProperty("secondary", True)
        export_btn.clicked.connect(self._export_theme)
        button_layout.addWidget(export_btn)
        
        button_layout.addStretch()
        
        # Right side buttons
        reset_btn = QPushButton("Reset")
        reset_btn.setProperty("secondary", True)
        reset_btn.clicked.connect(self._reset_theme)
        button_layout.addWidget(reset_btn)
        
        apply_btn = QPushButton("Apply")
        apply_btn.clicked.connect(self._apply_theme)
        button_layout.addWidget(apply_btn)
        
        save_btn = QPushButton("Save")
        save_btn.setProperty("success", True)
        save_btn.clicked.connect(self._save_theme)
        button_layout.addWidget(save_btn)
        
        close_btn = QPushButton("Close")
        close_btn.setProperty("secondary", True)
        close_btn.clicked.connect(self.close)
        button_layout.addWidget(close_btn)
        
        layout.addLayout(button_layout)
        
        self.setLayout(layout)
    
    def _create_main_colors_tab(self):
        """Create tab for main colors"""
        widget = QWidget()
        layout = QVBoxLayout()
        
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(QScrollArea.Shape.NoFrame)
        
        content = QWidget()
        form = QFormLayout()
        
        self.color_buttons['main_color'] = self._create_color_field("main_color")
        form.addRow("Main Color:", self.color_buttons['main_color'])
        
        self.color_buttons['secondary_color'] = self._create_color_field("secondary_color")
        form.addRow("Secondary Color:", self.color_buttons['secondary_color'])
        
        self.color_buttons['tertiary_color'] = self._create_color_field("tertiary_color")
        form.addRow("Tertiary Color:", self.color_buttons['tertiary_color'])
        
        self.color_buttons['background'] = self._create_color_field("background")
        form.addRow("Background:", self.color_buttons['background'])
        
        content.setLayout(form)
        scroll.setWidget(content)
        layout.addWidget(scroll)
        widget.setLayout(layout)
        return widget
    
    def _create_accent_colors_tab(self):
        """Create tab for accent colors"""
        widget = QWidget()
        layout = QVBoxLayout()

        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(QScrollArea.Shape.NoFrame)

        content = QWidget()
        form = QFormLayout()

        # Primary accent colors
        primary_label = QLabel("Primary Accent (Bright)")
        primary_label.setStyleSheet("font-weight: bold; font-size: 12px; margin-top: 10px;")
        form.addRow("", primary_label)

        self.color_buttons['accent_color'] = self._create_color_field("accent_color")
        form.addRow("Accent Color:", self.color_buttons['accent_color'])

        self.color_buttons['accent_hover'] = self._create_color_field("accent_hover")
        form.addRow("Accent Hover:", self.color_buttons['accent_hover'])

        self.color_buttons['accent_pressed'] = self._create_color_field("accent_pressed")
        form.addRow("Accent Pressed:", self.color_buttons['accent_pressed'])

        # Secondary accent colors
        secondary_label = QLabel("Secondary Accent (Muted)")
        secondary_label.setStyleSheet("font-weight: bold; font-size: 12px; margin-top: 15px;")
        form.addRow("", secondary_label)

        self.color_buttons['secondary_accent_color'] = self._create_color_field("secondary_accent_color")
        form.addRow("Secondary Accent:", self.color_buttons['secondary_accent_color'])

        self.color_buttons['secondary_accent_hover'] = self._create_color_field("secondary_accent_hover")
        form.addRow("Secondary Hover:", self.color_buttons['secondary_accent_hover'])

        self.color_buttons['secondary_accent_pressed'] = self._create_color_field("secondary_accent_pressed")
        form.addRow("Secondary Pressed:", self.color_buttons['secondary_accent_pressed'])

        content.setLayout(form)
        scroll.setWidget(content)
        layout.addWidget(scroll)
        widget.setLayout(layout)
        return widget
    
    def _create_status_colors_tab(self):
        """Create tab for status colors"""
        widget = QWidget()
        layout = QVBoxLayout()
        
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(QScrollArea.Shape.NoFrame)
        
        content = QWidget()
        form = QFormLayout()
        
        self.color_buttons['error_color'] = self._create_color_field("error_color")
        form.addRow("Error Color:", self.color_buttons['error_color'])
        
        self.color_buttons['warning_color'] = self._create_color_field("warning_color")
        form.addRow("Warning Color:", self.color_buttons['warning_color'])
        
        self.color_buttons['confirm_color'] = self._create_color_field("confirm_color")
        form.addRow("Confirm Color:", self.color_buttons['confirm_color'])
        
        self.color_buttons['success_color'] = self._create_color_field("success_color")
        form.addRow("Success Color:", self.color_buttons['success_color'])
        
        content.setLayout(form)
        scroll.setWidget(content)
        layout.addWidget(scroll)
        widget.setLayout(layout)
        widget.setLayout(layout)
        return widget
    
    def _create_ui_colors_tab(self):
        """Create tab for UI element colors"""
        widget = QWidget()
        layout = QVBoxLayout()
        
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(QScrollArea.Shape.NoFrame)
        
        content = QWidget()
        form = QFormLayout()
        
        self.color_buttons['surface'] = self._create_color_field("surface")
        form.addRow("Surface:", self.color_buttons['surface'])
        
        self.color_buttons['surface_hover'] = self._create_color_field("surface_hover")
        form.addRow("Surface Hover:", self.color_buttons['surface_hover'])
        
        self.color_buttons['border'] = self._create_color_field("border")
        form.addRow("Border:", self.color_buttons['border'])
        
        self.color_buttons['border_focus'] = self._create_color_field("border_focus")
        form.addRow("Border Focus:", self.color_buttons['border_focus'])
        
        content.setLayout(form)
        scroll.setWidget(content)
        layout.addWidget(scroll)
        widget.setLayout(layout)
        return widget
    
    def _create_text_colors_tab(self):
        """Create tab for text colors"""
        widget = QWidget()
        layout = QVBoxLayout()
        
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(QScrollArea.Shape.NoFrame)
        
        content = QWidget()
        form = QFormLayout()
        
        self.color_buttons['text_primary'] = self._create_color_field("text_primary")
        form.addRow("Primary Text:", self.color_buttons['text_primary'])
        
        self.color_buttons['text_secondary'] = self._create_color_field("text_secondary")
        form.addRow("Secondary Text:", self.color_buttons['text_secondary'])
        
        self.color_buttons['text_disabled'] = self._create_color_field("text_disabled")
        form.addRow("Disabled Text:", self.color_buttons['text_disabled'])
        
        self.color_buttons['text_on_accent'] = self._create_color_field("text_on_accent")
        form.addRow("Text on Accent:", self.color_buttons['text_on_accent'])
        
        content.setLayout(form)
        scroll.setWidget(content)
        layout.addWidget(scroll)
        widget.setLayout(layout)
        return widget
    
    def _create_special_colors_tab(self):
        """Create tab for special colors"""
        widget = QWidget()
        layout = QVBoxLayout()
        
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(QScrollArea.Shape.NoFrame)
        
        content = QWidget()
        form = QFormLayout()
        
        self.color_buttons['selection'] = self._create_color_field("selection")
        form.addRow("Selection:", self.color_buttons['selection'])
        
        self.color_buttons['selection_inactive'] = self._create_color_field("selection_inactive")
        form.addRow("Selection Inactive:", self.color_buttons['selection_inactive'])
        
        self.color_buttons['shadow'] = self._create_color_field("shadow")
        form.addRow("Shadow:", self.color_buttons['shadow'])
        
        content.setLayout(form)
        scroll.setWidget(content)
        layout.addWidget(scroll)
        widget.setLayout(layout)
        return widget
    
    def _create_spacing_tab(self):
        """Create tab for spacing and padding settings"""
        widget = QWidget()
        layout = QVBoxLayout()
        
        # Inspector Panel Spacing Group
        spacing_group = QGroupBox("Inspector Panel Spacing")
        spacing_form = QFormLayout()
        
        # Vertical spacing
        self.spacing_widgets['inspector_vertical_spacing'] = QSpinBox()
        self.spacing_widgets['inspector_vertical_spacing'].setRange(0, 50)
        self.spacing_widgets['inspector_vertical_spacing'].setSuffix(" px")
        self.spacing_widgets['inspector_vertical_spacing'].valueChanged.connect(self._on_spacing_changed)
        spacing_form.addRow("Vertical Spacing:", self.spacing_widgets['inspector_vertical_spacing'])
        
        v_label = QLabel("Space between property widgets vertically")
        v_label.setProperty("secondary", True)
        v_label.setStyleSheet("color: #888; font-size: 11px;")
        spacing_form.addRow("", v_label)
        
        # Horizontal padding
        self.spacing_widgets['inspector_horizontal_padding'] = QSpinBox()
        self.spacing_widgets['inspector_horizontal_padding'].setRange(0, 50)
        self.spacing_widgets['inspector_horizontal_padding'].setSuffix(" px")
        self.spacing_widgets['inspector_horizontal_padding'].valueChanged.connect(self._on_spacing_changed)
        spacing_form.addRow("Horizontal Padding:", self.spacing_widgets['inspector_horizontal_padding'])
        
        h_label = QLabel("Padding within property widgets and around content")
        h_label.setProperty("secondary", True)
        h_label.setStyleSheet("color: #888; font-size: 11px;")
        spacing_form.addRow("", h_label)
        
        spacing_group.setLayout(spacing_form)
        layout.addWidget(spacing_group)
        
        # Icon Pack Selection Group
        icon_group = QGroupBox("Icon Pack")
        icon_form = QFormLayout()
        
        # Icon pack dropdown
        self.icon_pack_combo = QComboBox()
        icon_manager = get_icon_pack_manager()
        available_packs = icon_manager.get_icon_packs()
        self.icon_pack_combo.addItems(available_packs)
        self.icon_pack_combo.currentTextChanged.connect(self._on_icon_pack_changed)
        icon_form.addRow("Icon Pack:", self.icon_pack_combo)
        
        icon_label = QLabel("Icon pack for scene tree visibility/enabled icons")
        icon_label.setProperty("secondary", True)
        icon_label.setStyleSheet("color: #888; font-size: 11px;")
        icon_form.addRow("", icon_label)
        
        # Custom icon pack path
        custom_layout = QHBoxLayout()
        self.custom_icon_path_edit = QLineEdit()
        self.custom_icon_path_edit.setPlaceholderText("Path to custom icon pack folder...")
        custom_layout.addWidget(self.custom_icon_path_edit)
        
        browse_btn = QPushButton("Browse...")
        browse_btn.setFixedWidth(80)
        browse_btn.clicked.connect(self._browse_custom_icon_pack)
        custom_layout.addWidget(browse_btn)
        
        icon_form.addRow("Custom Path:", custom_layout)
        
        custom_help = QLabel("Select a folder containing the 6 required icon files")
        custom_help.setProperty("secondary", True)
        custom_help.setStyleSheet("color: #888; font-size: 11px;")
        icon_form.addRow("", custom_help)
        
        icon_group.setLayout(icon_form)
        layout.addWidget(icon_group)
        
        layout.addStretch()
        widget.setLayout(layout)
        return widget
    
    def _create_color_field(self, color_name):
        """Create a color button for a specific color field"""
        color_value = getattr(self.current_theme.colors, color_name, "#000000")
        button = ColorButton(color_value)
        button.color_changed.connect(lambda c: self._on_color_changed(color_name, c))
        return button
    
    def _on_color_changed(self, color_name, color_hex):
        """Handle color change"""
        setattr(self.current_theme.colors, color_name, color_hex)
    
    def _on_spacing_changed(self):
        """Handle spacing change"""
        self.current_theme.colors.inspector_vertical_spacing = self.spacing_widgets['inspector_vertical_spacing'].value()
        self.current_theme.colors.inspector_horizontal_padding = self.spacing_widgets['inspector_horizontal_padding'].value()
    
    def _on_icon_pack_changed(self, pack_name):
        """Handle icon pack selection change"""
        self.current_theme.colors.icon_pack = pack_name
    
    def _browse_custom_icon_pack(self):
        """Browse for custom icon pack folder"""
        folder = QFileDialog.getExistingDirectory(
            self, "Select Icon Pack Folder",
            "",
            QFileDialog.Option.ShowDirsOnly
        )
        
        if folder:
            self.custom_icon_path_edit.setText(folder)
            # Register the custom pack
            icon_manager = get_icon_pack_manager()
            pack_name = Path(folder).name
            icon_manager.register_icon_pack(pack_name, folder)
            
            # Add to combo if not already there
            if self.icon_pack_combo.findText(pack_name) == -1:
                self.icon_pack_combo.addItem(pack_name)
            
            # Select the new pack
            self.icon_pack_combo.setCurrentText(pack_name)
            self.current_theme.colors.icon_pack = pack_name
    
    def _on_name_changed(self, name):
        """Handle theme name change"""
        self.current_theme.name = name
    
    def _on_theme_changed(self, theme_name):
        """Handle base theme selection change"""
        base_theme = self.theme_manager.get_theme(theme_name)
        if base_theme:
            self.current_theme = copy.deepcopy(base_theme)
            self.theme_name_edit.setText(base_theme.name)
            self._load_theme_values()
    
    def _load_theme_values(self):
        """Load current theme values into UI"""
        # Load colors
        for color_name, button in self.color_buttons.items():
            color_value = getattr(self.current_theme.colors, color_name, "#000000")
            button.set_color(color_value)
        
        # Load spacing
        self.spacing_widgets['inspector_vertical_spacing'].setValue(
            self.current_theme.colors.inspector_vertical_spacing
        )
        self.spacing_widgets['inspector_horizontal_padding'].setValue(
            self.current_theme.colors.inspector_horizontal_padding
        )
        
        # Load icon pack
        if hasattr(self, 'icon_pack_combo'):
            icon_pack = self.current_theme.colors.icon_pack
            index = self.icon_pack_combo.findText(icon_pack)
            if index >= 0:
                self.icon_pack_combo.setCurrentIndex(index)
    
    def _apply_theme(self):
        """Apply theme to see live preview"""
        from PyQt6.QtWidgets import QApplication
        self.theme_manager.update_theme(self.current_theme)
        self.theme_manager.set_theme(self.current_theme.name)
        self.theme_manager.apply_theme(QApplication.instance())
        self.theme_changed.emit(self.current_theme)
        QMessageBox.information(self, "Theme Applied", 
                               f"Theme '{self.current_theme.name}' has been applied.\n"
                               "Changes are temporary until saved.")
    
    def _save_theme(self):
        """Save theme permanently"""
        self.theme_manager.update_theme(self.current_theme)
        self.theme_manager.set_theme(self.current_theme.name)
        from PyQt6.QtWidgets import QApplication
        self.theme_manager.apply_theme(QApplication.instance())
        self.theme_changed.emit(self.current_theme)
        QMessageBox.information(self, "Theme Saved", 
                               f"Theme '{self.current_theme.name}' has been saved.")
    
    def _reset_theme(self):
        """Reset to original theme"""
        reply = QMessageBox.question(
            self, "Reset Theme",
            "Reset all changes to the original theme?",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
        )
        
        if reply == QMessageBox.StandardButton.Yes:
            self.current_theme = copy.deepcopy(self.original_theme)
            self.theme_name_edit.setText(self.current_theme.name)
            self.theme_combo.setCurrentText(self.current_theme.name)
            self._load_theme_values()
    
    def _export_theme(self):
        """Export theme to .editorTheme file"""
        file_path, _ = QFileDialog.getSaveFileName(
            self, "Export Theme", 
            f"{self.current_theme.name}.editorTheme",
            "Editor Theme Files (*.editorTheme);;All Files (*)"
        )
        
        if file_path:
            try:
                self.theme_manager.save_theme(self.current_theme, file_path)
                QMessageBox.information(self, "Export Successful", 
                                       f"Theme exported to:\n{file_path}")
            except Exception as e:
                QMessageBox.critical(self, "Export Failed", 
                                    f"Failed to export theme:\n{str(e)}")
    
    def _import_theme(self):
        """Import theme from .editorTheme file"""
        file_path, _ = QFileDialog.getOpenFileName(
            self, "Import Theme", "",
            "Editor Theme Files (*.editorTheme);;All Files (*)"
        )
        
        if file_path:
            try:
                imported_theme = self.theme_manager.load_theme(file_path)
                self.current_theme = copy.deepcopy(imported_theme)
                self.theme_name_edit.setText(self.current_theme.name)
                
                # Update combo box if needed
                if self.current_theme.name not in self.theme_manager.get_theme_names():
                    self.theme_combo.addItem(self.current_theme.name)
                self.theme_combo.setCurrentText(self.current_theme.name)
                
                self._load_theme_values()
                QMessageBox.information(self, "Import Successful", 
                                       f"Theme '{self.current_theme.name}' imported successfully.")
            except Exception as e:
                QMessageBox.critical(self, "Import Failed", 
                                    f"Failed to import theme:\n{str(e)}")
    
    def closeEvent(self, event):
        """Handle dialog close"""
        # Optionally restore original theme if not saved
        event.accept()
