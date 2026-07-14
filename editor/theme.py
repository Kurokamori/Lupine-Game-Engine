"""
Lupine Engine Theme System
Provides color schemes and styling for the editor interface
"""

from dataclasses import dataclass
from typing import Dict
from PyQt6.QtGui import QColor, QPalette
from PyQt6.QtWidgets import QApplication
import json
import os


@dataclass
class ThemeColors:
    """Color definitions for a theme"""
    # Primary colors
    main_color: str
    secondary_color: str
    tertiary_color: str

    # Accent colors
    accent_color: str
    accent_hover: str
    accent_pressed: str

    # Secondary accent colors (more muted)
    secondary_accent_color: str
    secondary_accent_hover: str
    secondary_accent_pressed: str

    # Status colors
    error_color: str
    warning_color: str
    confirm_color: str
    success_color: str
    
    # UI element colors
    background: str
    surface: str
    surface_hover: str
    border: str
    border_focus: str
    
    # Text colors
    text_primary: str
    text_secondary: str
    text_disabled: str
    text_on_accent: str
    
    # Special elements
    selection: str
    selection_inactive: str
    shadow: str
    
    # Spacing and padding
    inspector_vertical_spacing: int = 8
    inspector_horizontal_padding: int = 6

    # Inspector layout tokens (property-row metrics and component-card chrome)
    inspector_label_width: int = 128
    inspector_control_height: int = 26
    inspector_card_radius: int = 6
    inspector_spine_width: int = 3

    # Icon pack
    icon_pack: str = "dark"


class Theme:
    """Represents a complete editor theme"""
    
    def __init__(self, name: str, colors: ThemeColors):
        self.name = name
        self.colors = colors
    
    def to_dict(self) -> Dict:
        """Convert theme to dictionary for serialization"""
        return {
            "name": self.name,
            "colors": {
                "main_color": self.colors.main_color,
                "secondary_color": self.colors.secondary_color,
                "tertiary_color": self.colors.tertiary_color,
                "accent_color": self.colors.accent_color,
                "accent_hover": self.colors.accent_hover,
                "accent_pressed": self.colors.accent_pressed,
                "secondary_accent_color": self.colors.secondary_accent_color,
                "secondary_accent_hover": self.colors.secondary_accent_hover,
                "secondary_accent_pressed": self.colors.secondary_accent_pressed,
                "error_color": self.colors.error_color,
                "warning_color": self.colors.warning_color,
                "confirm_color": self.colors.confirm_color,
                "success_color": self.colors.success_color,
                "background": self.colors.background,
                "surface": self.colors.surface,
                "surface_hover": self.colors.surface_hover,
                "border": self.colors.border,
                "border_focus": self.colors.border_focus,
                "text_primary": self.colors.text_primary,
                "text_secondary": self.colors.text_secondary,
                "text_disabled": self.colors.text_disabled,
                "text_on_accent": self.colors.text_on_accent,
                "selection": self.colors.selection,
                "selection_inactive": self.colors.selection_inactive,
                "shadow": self.colors.shadow,
                "inspector_vertical_spacing": self.colors.inspector_vertical_spacing,
                "inspector_horizontal_padding": self.colors.inspector_horizontal_padding,
                "inspector_label_width": self.colors.inspector_label_width,
                "inspector_control_height": self.colors.inspector_control_height,
                "inspector_card_radius": self.colors.inspector_card_radius,
                "inspector_spine_width": self.colors.inspector_spine_width,
                "icon_pack": self.colors.icon_pack,
            }
        }
    
    @staticmethod
    def from_dict(data: Dict) -> 'Theme':
        """Load theme from dictionary"""
        # Handle optional spacing fields for backwards compatibility
        color_data = data["colors"]
        if "inspector_vertical_spacing" not in color_data:
            color_data["inspector_vertical_spacing"] = 8
        if "inspector_horizontal_padding" not in color_data:
            color_data["inspector_horizontal_padding"] = 6
        if "icon_pack" not in color_data:
            color_data["icon_pack"] = "dark"
        # Inspector layout tokens (added later; default for older saved themes)
        if "inspector_label_width" not in color_data:
            color_data["inspector_label_width"] = 128
        if "inspector_control_height" not in color_data:
            color_data["inspector_control_height"] = 26
        if "inspector_card_radius" not in color_data:
            color_data["inspector_card_radius"] = 6
        if "inspector_spine_width" not in color_data:
            color_data["inspector_spine_width"] = 3
        # Handle secondary accent colors for backwards compatibility
        if "secondary_accent_color" not in color_data:
            color_data["secondary_accent_color"] = color_data.get("accent_color", "#7b5fb8")
        if "secondary_accent_hover" not in color_data:
            color_data["secondary_accent_hover"] = color_data.get("accent_hover", "#8f6fcc")
        if "secondary_accent_pressed" not in color_data:
            color_data["secondary_accent_pressed"] = color_data.get("accent_pressed", "#6750a0")
        colors = ThemeColors(**color_data)
        return Theme(data["name"], colors)
    
    def get_stylesheet(self) -> str:
        """Generate Qt stylesheet from theme colors"""
        c = self.colors

        # Tinted vector glyphs for spin-box / combo arrows (the QSS border-triangle trick
        # renders as a blank box for these sub-controls on many Qt builds).
        from widgets.inspector_icons import qss_url
        arrow_up = qss_url("chevron-up", c.text_secondary)
        arrow_down = qss_url("chevron-down", c.text_secondary)
        arrow_up_active = qss_url("chevron-up", c.text_primary)
        arrow_down_active = qss_url("chevron-down", c.text_primary)

        return f"""
            /* Main Window */
            QMainWindow, QDialog {{
                background-color: {c.background};
                color: {c.text_primary};
            }}
            
            /* Widgets */
            QWidget {{
                background-color: {c.background};
                color: {c.text_primary};
                border: none;
            }}
            
            /* Labels */
            QLabel {{
                background-color: transparent;
                color: {c.text_primary};
            }}
            
            QLabel[secondary="true"] {{
                color: {c.text_secondary};
            }}
            
            /* Buttons */
            QPushButton {{
                background-color: {c.accent_color};
                color: {c.text_on_accent};
                border: 1px solid {c.border};
                border-radius: 2px;
                padding: 6px 14px;
                font-weight: 500;
            }}
            
            QPushButton:hover {{
                background-color: {c.accent_hover};
                border-color: {c.border_focus};
            }}
            
            QPushButton:pressed {{
                background-color: {c.accent_pressed};
            }}
            
            QPushButton:disabled {{
                background-color: {c.surface};
                color: {c.text_disabled};
            }}
            
            QPushButton[secondary="true"] {{
                background-color: {c.surface};
                color: {c.text_primary};
                border: 1px solid {c.border};
            }}
            
            QPushButton[secondary="true"]:hover {{
                background-color: {c.surface_hover};
            }}
            
            QPushButton[danger="true"] {{
                background-color: {c.error_color};
                color: {c.text_on_accent};
            }}
            
            QPushButton[success="true"] {{
                background-color: {c.confirm_color};
                color: {c.text_on_accent};
            }}
            
            /* Line Edits */
            QLineEdit {{
                background-color: {c.surface};
                color: {c.text_primary};
                border: 1px solid {c.border};
                border-radius: 2px;
                padding: 6px 10px;
            }}
            
            QLineEdit:focus {{
                border-color: {c.border_focus};
            }}
            
            QLineEdit:disabled {{
                color: {c.text_disabled};
                background-color: {c.secondary_color};
            }}
            
            /* List Widgets */
            QListWidget {{
                background-color: {c.surface};
                color: {c.text_primary};
                border: 1px solid {c.border};
                border-radius: 0px;
                outline: none;
            }}
            
            QListWidget::item {{
                padding: 10px;
                border-bottom: 1px solid {c.border};
            }}
            
            QListWidget::item:hover {{
                background-color: {c.surface_hover};
            }}
            
            QListWidget::item:selected {{
                background-color: {c.selection};
                color: {c.text_primary};
            }}
            
            QListWidget::item:selected:!active {{
                background-color: {c.selection_inactive};
            }}
            
            /* Scroll Bars */
            QScrollBar:vertical {{
                background-color: {c.surface};
                width: 10px;
                border-radius: 0px;
            }}
            
            QScrollBar::handle:vertical {{
                background-color: {c.border};
                border-radius: 0px;
                min-height: 20px;
            }}
            
            QScrollBar::handle:vertical:hover {{
                background-color: {c.border_focus};
            }}
            
            QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {{
                height: 0px;
            }}
            
            QScrollBar:horizontal {{
                background-color: {c.surface};
                height: 10px;
                border-radius: 0px;
            }}
            
            QScrollBar::handle:horizontal {{
                background-color: {c.border};
                border-radius: 0px;
                min-width: 20px;
            }}
            
            QScrollBar::handle:horizontal:hover {{
                background-color: {c.border_focus};
            }}
            
            QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {{
                width: 0px;
            }}
            
            /* Group Boxes */
            QGroupBox {{
                background-color: {c.surface};
                border: 1px solid {c.border};
                border-radius: 0px;
                margin-top: 10px;
                padding-top: 10px;
                font-weight: 500;
            }}
            
            QGroupBox::title {{
                color: {c.text_primary};
                subcontrol-origin: margin;
                left: 10px;
                padding: 0 5px;
            }}
            
            /* ComboBox */
            QComboBox {{
                background-color: {c.surface};
                color: {c.text_primary};
                border: 1px solid {c.border};
                border-radius: 2px;
                padding: 6px 10px;
            }}
            
            QComboBox:focus {{
                border-color: {c.border_focus};
            }}
            
            QComboBox::drop-down {{
                border: none;
            }}
            
            QComboBox::drop-down {{
                width: 20px;
            }}

            QComboBox::down-arrow {{
                image: url("{arrow_down}");
                width: 10px;
                height: 10px;
                margin-right: 8px;
            }}

            QComboBox::down-arrow:on {{
                image: url("{arrow_down_active}");
            }}
            
            QComboBox QAbstractItemView {{
                background-color: {c.surface};
                color: {c.text_primary};
                border: 1px solid {c.border};
                selection-background-color: {c.selection};
            }}

            /* Spin Boxes (numeric property fields) */
            QSpinBox, QDoubleSpinBox {{
                background-color: {c.surface};
                color: {c.text_primary};
                border: 1px solid {c.border};
                border-radius: 3px;
                padding: 3px 6px;
                selection-background-color: {c.selection};
                selection-color: {c.text_primary};
            }}

            QSpinBox:hover, QDoubleSpinBox:hover {{
                border-color: {c.secondary_accent_color};
            }}

            QSpinBox:focus, QDoubleSpinBox:focus {{
                border-color: {c.border_focus};
            }}

            QSpinBox:disabled, QDoubleSpinBox:disabled {{
                color: {c.text_disabled};
                background-color: {c.secondary_color};
            }}

            QSpinBox::up-button, QDoubleSpinBox::up-button {{
                subcontrol-origin: border;
                subcontrol-position: top right;
                width: 16px;
                border-left: 1px solid {c.border};
                border-top-right-radius: 3px;
                background-color: {c.surface};
            }}

            QSpinBox::down-button, QDoubleSpinBox::down-button {{
                subcontrol-origin: border;
                subcontrol-position: bottom right;
                width: 16px;
                border-left: 1px solid {c.border};
                border-bottom-right-radius: 3px;
                background-color: {c.surface};
            }}

            QSpinBox::up-button:hover, QDoubleSpinBox::up-button:hover,
            QSpinBox::down-button:hover, QDoubleSpinBox::down-button:hover {{
                background-color: {c.surface_hover};
            }}

            QSpinBox::up-button:pressed, QDoubleSpinBox::up-button:pressed,
            QSpinBox::down-button:pressed, QDoubleSpinBox::down-button:pressed {{
                background-color: {c.secondary_accent_color};
            }}

            QSpinBox::up-arrow, QDoubleSpinBox::up-arrow {{
                image: url("{arrow_up}");
                width: 9px;
                height: 9px;
            }}

            QSpinBox::down-arrow, QDoubleSpinBox::down-arrow {{
                image: url("{arrow_down}");
                width: 9px;
                height: 9px;
            }}

            QSpinBox::up-arrow:hover, QDoubleSpinBox::up-arrow:hover {{
                image: url("{arrow_up_active}");
            }}

            QSpinBox::down-arrow:hover, QDoubleSpinBox::down-arrow:hover {{
                image: url("{arrow_down_active}");
            }}

            /* Dock Widgets */
            QDockWidget {{
                titlebar-close-icon: url(close.png);
                titlebar-normal-icon: url(float.png);
                border: 1px solid {c.border};
            }}
            
            QDockWidget::title {{
                background-color: {c.secondary_color};
                color: {c.text_primary};
                padding: 6px;
                border-bottom: 1px solid {c.border};
                font-weight: 500;
            }}
            
            QDockWidget::close-button, QDockWidget::float-button {{
                background-color: transparent;
                border: none;
                padding: 2px;
            }}
            
            QDockWidget::close-button:hover, QDockWidget::float-button:hover {{
                background-color: {c.surface_hover};
            }}
            
            /* Tab Widgets and Tab Bars */
            QTabWidget::pane {{
                border: 1px solid {c.border};
                background-color: {c.background};
            }}
            
            QTabBar {{
                background-color: {c.secondary_color};
            }}
            
            QTabBar::tab {{
                background-color: {c.surface};
                color: {c.text_secondary};
                border: 1px solid {c.border};
                border-bottom: none;
                padding: 8px 14px;
                margin-right: 2px;
                min-width: 80px;
            }}
            
            QTabBar::tab:selected {{
                background-color: {c.background};
                color: {c.text_primary};
                border-bottom: 2px solid {c.accent_color};
                font-weight: 500;
            }}
            
            QTabBar::tab:hover:!selected {{
                background-color: {c.surface_hover};
            }}
            
            QTabBar::tab:!selected {{
                margin-top: 2px;
            }}
            
            QTabBar::scroller {{
                width: 20px;
            }}
            
            /* Toolbars */
            QToolBar {{
                background-color: {c.secondary_color};
                border-bottom: 1px solid {c.border};
                spacing: 6px;
                padding: 4px;
            }}
            
            QToolBar::separator {{
                background-color: {c.border};
                width: 1px;
                margin: 4px 8px;
            }}
            
            /* Status Bar */
            QStatusBar {{
                background-color: {c.secondary_color};
                color: {c.text_secondary};
                border-top: 1px solid {c.border};
            }}
            
            /* File Dialog */
            QFileDialog {{
                background-color: {c.background};
                color: {c.text_primary};
            }}
            
            /* Tool Tips */
            QToolTip {{
                background-color: {c.surface};
                color: {c.text_primary};
                border: 1px solid {c.border};
                padding: 4px;
            }}
            
            /* Menu Bar */
            QMenuBar {{
                background-color: {c.main_color};
                color: {c.text_primary};
                border-bottom: 1px solid {c.border};
            }}
            
            QMenuBar::item:selected {{
                background-color: {c.surface_hover};
            }}
            
            QMenu {{
                background-color: {c.surface};
                color: {c.text_primary};
                border: 1px solid {c.border};
            }}
            
            QMenu::item:selected {{
                background-color: {c.selection};
            }}
            
            /* Checkboxes */
            QCheckBox {{
                background-color: transparent;
                color: {c.text_primary};
                spacing: 8px;
            }}

            QCheckBox::indicator {{
                width: 16px;
                height: 16px;
                border: 2px solid {c.border};
                border-radius: 3px;
                background-color: {c.surface};
            }}

            QCheckBox::indicator:hover {{
                border-color: {c.accent_color};
                background-color: {c.surface_hover};
            }}

            QCheckBox::indicator:checked {{
                background-color: {c.accent_color};
                border-color: {c.accent_color};
                border-width: 2px;
                border-style: solid;
            }}

            QCheckBox::indicator:checked:hover {{
                background-color: {c.accent_hover};
                border-color: {c.accent_hover};
            }}

            QCheckBox::indicator:disabled {{
                border-color: {c.border};
                background-color: {c.secondary_color};
            }}

            QCheckBox:disabled {{
                color: {c.text_disabled};
            }}

            /* Radio Buttons */
            QRadioButton {{
                background-color: transparent;
                color: {c.text_primary};
                spacing: 8px;
            }}

            QRadioButton::indicator {{
                width: 16px;
                height: 16px;
                border: 2px solid {c.border};
                border-radius: 10px;
                background-color: {c.surface};
            }}

            QRadioButton::indicator:hover {{
                border-color: {c.accent_color};
                background-color: {c.surface_hover};
            }}

            QRadioButton::indicator:checked {{
                background-color: {c.text_on_accent};
                border: 5px solid {c.accent_color};
            }}

            QRadioButton::indicator:checked:hover {{
                background-color: {c.text_on_accent};
                border: 5px solid {c.accent_hover};
            }}

            QRadioButton::indicator:disabled {{
                border-color: {c.border};
                background-color: {c.secondary_color};
            }}

            QRadioButton:disabled {{
                color: {c.text_disabled};
            }}
        """


class ThemeManager:
    """Manages themes for the Lupine Engine editor"""
    
    def __init__(self):
        self.themes: Dict[str, Theme] = {}
        self.current_theme: Theme = None
        self._load_default_themes()
    
    def _load_default_themes(self):
        """Load built-in themes"""
        # Dark Purple Theme (Default)
        dark_purple = Theme(
            name="Dark Purple",
            colors=ThemeColors(
                # Primary colors - Purplish dark grays
                main_color="#19171E",
                secondary_color="#1e1b24",
                tertiary_color="#232029",

                # Accent colors - Bright purple
                accent_color="#76669b",
                accent_hover="#72648c",
                accent_pressed="#49405e",

                # Secondary accent colors - Muted purple/gray
                secondary_accent_color="#373246",
                secondary_accent_hover="#332e3d",
                secondary_accent_pressed="#201c2a",

                # Status colors
                error_color="#d32f2f",
                warning_color="#f57c00",
                confirm_color="#388e3c",
                success_color="#4caf50",
                
                # UI elements
                background="#181719",
                surface="#141317",
                surface_hover="#0c0c0f",
                border="#27242f",
                border_focus="#514864",
                
                # Text colors
                text_primary="#e8e6f0",
                text_secondary="#b0a8c0",
                text_disabled="#6a6178",
                text_on_accent="#ffffff",
                
                # Special
                selection="#3e384f",
                selection_inactive="#332d45",
                shadow="#0a0810",
                
                # Spacing and padding
                inspector_vertical_spacing=8,
                inspector_horizontal_padding=6,
                
                # Icon pack
                icon_pack="dark",
            )
        )
        
        # Light Theme
        light = Theme(
            name="Light",
            colors=ThemeColors(
                main_color="#f5f5f5",
                secondary_color="#e0e0e0",
                tertiary_color="#d0d0d0",

                # Accent colors - Bright purple
                accent_color="#7b5fb8",
                accent_hover="#8f6fcc",
                accent_pressed="#6750a0",

                # Secondary accent colors - Muted purple/gray
                secondary_accent_color="#9b8fc8",
                secondary_accent_hover="#ab9fd8",
                secondary_accent_pressed="#8b7fb8",

                error_color="#d32f2f",
                warning_color="#f57c00",
                confirm_color="#388e3c",
                success_color="#4caf50",
                
                background="#fafafa",
                surface="#ffffff",
                surface_hover="#f0f0f0",
                border="#c0c0c0",
                border_focus="#7b5fb8",
                
                text_primary="#1a1a1a",
                text_secondary="#606060",
                text_disabled="#a0a0a0",
                text_on_accent="#ffffff",
                
                selection="#d0c0e8",
                selection_inactive="#e0e0e0",
                shadow="#00000020",
                
                # Spacing and padding
                inspector_vertical_spacing=8,
                inspector_horizontal_padding=6,
                
                # Icon pack
                icon_pack="light",
            )
        )
        
        self.themes["Dark Purple"] = dark_purple
        self.themes["Light"] = light
        self.current_theme = dark_purple
    
    def get_theme(self, name: str) -> Theme:
        """Get a theme by name"""
        return self.themes.get(name)
    
    def get_current_theme(self) -> Theme:
        """Get the currently active theme"""
        return self.current_theme
    
    def set_theme(self, name: str):
        """Set the active theme by name"""
        if name in self.themes:
            self.current_theme = self.themes[name]
    
    def apply_theme(self, app: QApplication):
        """Apply the current theme to the application"""
        if self.current_theme:
            # Clear stylesheet first to force Qt to refresh
            app.setStyleSheet("")
            # Now apply the new stylesheet
            app.setStyleSheet(self.current_theme.get_stylesheet())
    
    def get_theme_names(self) -> list:
        """Get list of available theme names"""
        return list(self.themes.keys())
    
    def save_theme(self, theme: Theme, filepath: str):
        """Save a theme to a JSON file"""
        with open(filepath, 'w') as f:
            json.dump(theme.to_dict(), f, indent=2)
    
    def load_theme(self, filepath: str) -> Theme:
        """Load a theme from a JSON file"""
        with open(filepath, 'r') as f:
            data = json.load(f)
            theme = Theme.from_dict(data)
            self.themes[theme.name] = theme
            return theme
    
    def add_theme(self, theme: Theme):
        """Add a theme to the manager"""
        self.themes[theme.name] = theme
    
    def update_theme(self, theme: Theme):
        """Update an existing theme"""
        self.themes[theme.name] = theme
        if self.current_theme and self.current_theme.name == theme.name:
            self.current_theme = theme


# Global theme manager instance
_theme_manager = None

def get_theme_manager() -> ThemeManager:
    """Get the global theme manager instance"""
    global _theme_manager
    if _theme_manager is None:
        _theme_manager = ThemeManager()
    return _theme_manager


class IconPackManager:
    """Manages icon packs for the editor"""
    
    def __init__(self):
        from pathlib import Path
        self.base_path = Path(__file__).parent / "assets" / "icons"
        self._icon_packs = {}
        self._scan_icon_packs()
    
    def _scan_icon_packs(self):
        """Scan the icons directory for available icon packs"""
        if not self.base_path.exists():
            return
        
        # Scan for subdirectories in the icons folder
        for pack_dir in self.base_path.iterdir():
            if pack_dir.is_dir():
                pack_name = pack_dir.name
                # Check if it has the required icons
                if self._validate_icon_pack(pack_dir):
                    self._icon_packs[pack_name] = str(pack_dir)
    
    def _validate_icon_pack(self, pack_path) -> bool:
        """Check if an icon pack has all required icons"""
        required_icons = [
            "visible.png",
            "invisible.png", 
            "parent-invisible.png",
            "enabled.png",
            "disabled.png",
            "parent-disabled.png"
        ]
        
        # For now, just check if the directory exists
        # Icons can be added later
        return True
    
    def get_icon_packs(self) -> list:
        """Get list of available icon pack names"""
        return list(self._icon_packs.keys())
    
    def get_icon_pack_path(self, pack_name: str) -> str:
        """Get the path to an icon pack"""
        return self._icon_packs.get(pack_name, "")
    
    def register_icon_pack(self, pack_name: str, pack_path: str):
        """Register a custom icon pack"""
        from pathlib import Path
        path = Path(pack_path)
        if path.exists() and path.is_dir():
            self._icon_packs[pack_name] = str(path)
    
    def get_icon_path(self, pack_name: str, icon_name: str) -> str:
        """Get the full path to a specific icon in a pack"""
        pack_path = self.get_icon_pack_path(pack_name)
        if pack_path:
            from pathlib import Path
            return str(Path(pack_path) / icon_name)
        return ""


# Global icon pack manager instance
_icon_pack_manager = None

def get_icon_pack_manager() -> IconPackManager:
    """Get the global icon pack manager instance"""
    global _icon_pack_manager
    if _icon_pack_manager is None:
        _icon_pack_manager = IconPackManager()
    return _icon_pack_manager
