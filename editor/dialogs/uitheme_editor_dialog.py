"""
UI Theme Editor Dialog

Authoring window for the engine's Godot-style UI Theme system. It edits a
`.uitheme` JSON asset (and, optionally, an external `.palette` asset) that the
runtime `ThemeManager` consumes to style every UI control from one place.

The dialog is organized around the design-token model:

  * LEFT   - tokens: a color palette, scalar variables, and font roles.
  * CENTER - a live, engine-rendered preview of real UI controls (a throwaway
             scene rendered through an embedded ViewportWidget). When the engine
             path is unavailable it falls back to an approximate Qt-drawn gallery.
  * RIGHT  - the selected control type's themed entries, each with a binding
             chip that flips the value between literal / palette / variable /
             inherited.

It reads and writes the same JSON files the runtime loads, mirroring the
load/save/signal shape of `editor/dialogs/localization_editor_dialog.py`.
"""

import json
import os
import sys
from pathlib import Path
from typing import Dict, List, Optional, Any

from PyQt6.QtWidgets import (QDialog, QVBoxLayout, QHBoxLayout, QGridLayout,
                             QPushButton, QLabel, QWidget, QLineEdit, QComboBox,
                             QDoubleSpinBox, QCheckBox, QMessageBox, QFileDialog,
                             QScrollArea, QFrame, QMenu, QInputDialog)
from PyQt6.QtCore import Qt, pyqtSignal, QTimer
from PyQt6.QtGui import QColor

# The editor adds its own directory to sys.path; mirror theme_editor_dialog so the
# `dialogs.*` / `panels.*` imports resolve when this module is loaded standalone.
sys.path.insert(0, str(Path(__file__).parent.parent))

from dialogs.theme_editor_dialog import ColorButton
from panels.inspector_panel import (CollapsibleGroupBox, PathPropertyWidget,
                                     convert_to_res_path)

# The center column prefers a live, engine-rendered preview using a throwaway
# scene of real UI controls. If the viewport widget cannot be imported (e.g. the
# dialog is exercised without the editor runtime), the dialog falls back to the
# Qt approximation gallery.
try:
    from viewport_widget import ViewportWidget
    _VIEWPORT_AVAILABLE = True
except Exception as _viewport_import_exc:
    ViewportWidget = None
    _VIEWPORT_AVAILABLE = False
    print(f"UI Theme: ViewportWidget unavailable, preview will use Qt gallery: "
          f"{_viewport_import_exc}")


# ---------------------------------------------------------------------------
# Type catalog: TypeName -> ordered { entry_name: kind } where kind is one of
# "color", "constant", "font", "image". Mirrors docs/UI_THEME_SYSTEM.md section 11.
# ---------------------------------------------------------------------------

TYPE_CATALOG: Dict[str, Dict[str, str]] = {
    "Button": {"background": "color", "border_color": "color", "font_color": "color",
               "corner_radius": "constant", "font_size": "constant", "font": "font",
               "background_image": "image"},
    "ToggleButton": {"background": "color", "border_color": "color", "font_color": "color",
                     "corner_radius": "constant", "font_size": "constant", "font": "font"},
    "TextureButton": {"font_color": "color", "font_size": "constant", "font": "font",
                      "texture": "image", "normal_texture": "image", "hover_texture": "image",
                      "pressed_texture": "image", "disabled_texture": "image"},
    "Panel": {"background": "color", "border_color": "color", "shadow_color": "color",
              "corner_radius": "constant", "background_image": "image"},
    "ColorRect": {"background": "color", "border_color": "color", "corner_radius": "constant"},
    "Image2D": {"modulate": "color", "border_color": "color", "corner_radius": "constant"},
    "NineSlicePanel": {"modulate": "color"},
    "Container": {"background": "color", "border_color": "color", "corner_radius": "constant"},
    "Label": {"font_color": "color", "outline_color": "color",
              "font_size": "constant", "font": "font"},
    "RichTextLabel": {"font_color": "color", "font_size": "constant", "font": "font"},
    "LineEdit": {"font_color": "color", "placeholder_color": "color", "background": "color",
                 "border_color": "color", "selection_color": "color", "caret_color": "color",
                 "font_size": "constant", "corner_radius": "constant", "border_width": "constant",
                 "font": "font"},
    "TextEdit": {"font_color": "color", "placeholder_color": "color", "background": "color",
                 "border_color": "color", "selection_color": "color", "caret_color": "color",
                 "font_size": "constant", "corner_radius": "constant", "border_width": "constant",
                 "font": "font"},
    "SpinBox": {"font_color": "color", "background": "color", "button_color": "color",
                "border_color": "color", "font_size": "constant", "corner_radius": "constant",
                "border_width": "constant", "font": "font"},
    "Checkbox": {"background": "color", "border_color": "color", "checkmark_color": "color",
                 "font_color": "color", "font_size": "constant", "corner_radius": "constant",
                 "border_width": "constant", "font": "font"},
    "RadioButton": {"background": "color", "border_color": "color", "inner_circle_color": "color",
                    "font_color": "color", "font_size": "constant", "border_width": "constant",
                    "font": "font"},
    "ItemList": {"background": "color", "border_color": "color", "font_color": "color",
                 "selection_color": "color", "hover_color": "color", "font_size": "constant",
                 "border_width": "constant", "font": "font"},
    "Tree": {"background": "color", "border_color": "color", "font_color": "color",
             "selection_color": "color", "hover_color": "color", "font_size": "constant",
             "border_width": "constant", "font": "font"},
    "Dropdown": {"background": "color", "border_color": "color", "font_color": "color",
                 "hover_color": "color", "popup_background": "color", "font_size": "constant",
                 "corner_radius": "constant", "border_width": "constant", "font": "font"},
    "PopupMenu": {"background": "color", "border_color": "color", "font_color": "color",
                  "hover_color": "color", "font_size": "constant", "font": "font"},
    "Slider": {"track_color": "color", "fill_color": "color", "grabber_color": "color",
               "track_thickness": "constant", "grabber_radius": "constant"},
    "ScrollContainer": {"scrollbar_color": "color", "scrollbar_background": "color",
                        "background": "color", "border_color": "color", "corner_radius": "constant"},
    "TabContainer": {"tab_color": "color", "tab_selected_color": "color", "tab_bar_color": "color",
                     "font_color": "color", "background": "color", "border_color": "color",
                     "font_size": "constant", "corner_radius": "constant", "font": "font"},
    "ProgressBar": {"background": "color", "fill_color": "color", "border_color": "color",
                    "font_color": "color", "font_size": "constant", "corner_radius": "constant"},
}

# kind -> the JSON category that holds it under a type.
KIND_CATEGORY: Dict[str, str] = {"color": "colors", "constant": "constants",
                                 "font": "fonts", "vec2": "vec2s", "bool": "bools",
                                 "image": "images"}

# kind -> collapsible group title shown on the right panel.
KIND_GROUP_TITLE: Dict[str, str] = {"color": "Colors", "constant": "Sizes", "font": "Fonts",
                                    "image": "Images"}

COLOR_OPS: List[str] = ["none", "lighten", "darken", "alpha", "saturate", "desaturate"]

# StyleBox subtypes the authoring section can create. The strings must match the
# C++ StyleBox::GetTypeName values so CreateFromJson resolves the right subclass.
STYLEBOX_TYPES: List[str] = ["StyleBoxFlat", "StyleBoxTexture",
                             "StyleBoxLine", "StyleBoxEmpty"]

# AxisStretchMode strings accepted by StyleBoxTexture (Theme/StyleBox.cpp).
AXIS_STRETCH_MODES: List[str] = ["stretch", "tile", "tile_fit"]

# ---------------------------------------------------------------------------
# Per-state settings for stateful controls. Each control type maps to its
# ordered interaction states; "tween" indicates the type also carries per-state
# tween entries (scale/rotation/position/duration/enabled). These are authored
# under a "States" section and resolved by the runtime exactly like the base
# entries, with the theme entry name matching the component property name.
# ---------------------------------------------------------------------------

STATE_CATALOG: Dict[str, Dict[str, Any]] = {
    "Button": {"states": ["normal", "hover", "pressed", "disabled"], "tween": True},
    "ToggleButton": {"states": ["normal", "hover", "pressed", "toggled",
                                "toggledHover", "toggledPressed", "disabled"], "tween": True},
    "TextureButton": {"states": ["normal", "hover", "pressed", "disabled"], "tween": True},
    "TextureToggleButton": {"states": ["normal", "hover", "pressed", "toggled",
                                       "toggledHover", "toggledPressed", "disabled"], "tween": True},
    "Checkbox": {"states": ["normal", "hover", "pressed", "checked",
                            "checkedHover", "disabled"], "tween": False},
    "RadioButton": {"states": ["normal", "hover", "pressed", "selected",
                               "selectedHover", "disabled"], "tween": False},
}

# Per-state tween entry suffix -> kind (modulation is handled separately as a
# multiplier color). Order here is the order shown under each state group.
STATE_TWEEN_ENTRIES: List[tuple] = [
    ("TweenEnabled", "bool"),
    ("TweenScale", "vec2"),
    ("TweenRotation", "constant"),
    ("TweenPosition", "vec2"),
    ("TweenDuration", "constant"),
]


def _humanize_camel(text: str) -> str:
    """Turn a camelCase token into a spaced, title-cased label (toggledHover -> Toggled Hover)."""
    out: List[str] = []
    for i, ch in enumerate(text):
        if ch.isupper() and i > 0 and not text[i - 1].isupper():
            out.append(" ")
        out.append(ch)
    return "".join(out).strip().title()


def _clamp01(value: float) -> float:
    """Clamp a float into the inclusive 0..1 range."""
    if value < 0.0:
        return 0.0
    if value > 1.0:
        return 1.0
    return value


def _to_255(value: float) -> int:
    """Convert a 0..1 channel into an 8-bit integer."""
    return int(round(_clamp01(float(value)) * 255))


class UIThemeEditorDialog(QDialog):
    """Editor window for a `.uitheme` asset and its optional `.palette`."""

    # Emitted with the saved .uitheme absolute path once a save succeeds.
    theme_changed = pyqtSignal(str)

    def __init__(self, project, editor_bridge=None, parent=None, theme_path=None,
                 editor_session=None):
        super().__init__(parent)
        self.project = project
        self.editor_bridge = editor_bridge
        self.editor_session = editor_session
        self.project_dir: str = project.get_directory() if project else ""

        # Live engine preview state. When an editor session and bridge are
        # available these hold a throwaway ViewportWidget rendering a detached
        # scene of real UI controls; otherwise they stay None and the dialog
        # uses the Qt approximation gallery.
        self._preview_viewport = None
        self._preview_scene = None
        # Each entry is a record: {"node", "comp", "type", "name", "extra",
        # "width", "height"} so the layout can be read back and persisted.
        self._preview_controls: List[Dict[str, Any]] = []
        self._use_engine_preview: bool = False
        # The control node currently selected in the preview viewport (for removal).
        self._preview_selected_node = None
        # Tracks whether we disabled the bridge's undo recording for this dialog.
        self._undo_recording_suppressed: bool = False
        # One-shot guard so a double close (done() + closeEvent) tears down once.
        self._preview_torn_down: bool = False

        # In-memory model. self.theme is the parsed .uitheme; tokens that are
        # awkward to edit as dicts (variables, font roles) are mirrored into
        # ordered lists so rows can hold stable object references.
        self.theme: Dict[str, Any] = {}
        self.palette_slots: List[Dict[str, Any]] = []
        self.palette_mode: str = "inline"          # "none" | "inline" | "external"
        self.palette_ext_res: str = ""             # res:// path when external
        self.palette_name: str = "Palette"
        self.theme_path: str = ""                  # absolute path; empty for a new theme
        self._variables: List[Dict[str, Any]] = []     # [{"key": str, "value": float}]
        self._font_roles: List[Dict[str, Any]] = []    # [{"role": str, "font": str, "size": float}]
        self._loading: bool = True

        self.setWindowTitle("UI Theme")
        self.setModal(True)
        self.setMinimumSize(1100, 720)

        self._setup_ui()

        if theme_path:
            abs_path = self._res_to_abs(str(theme_path))
            if abs_path.lower().endswith(".palette"):
                # Opened a .palette directly: start a fresh theme that references
                # this palette externally and edit its slots, so a save writes the
                # palette file back in place.
                self._new_theme()
                self.palette_mode = "external"
                self.palette_ext_res = self._abs_to_res(abs_path)
                self._load_external_palette(self.palette_ext_res)
            else:
                self._load_theme(abs_path)
        else:
            found = self._find_first_theme()
            if found:
                self._load_theme(found)
            else:
                self._new_theme()

        self._refresh_all()

    # ------------------------------------------------------------------ UI

    def _setup_ui(self) -> None:
        layout = QVBoxLayout()

        title = QLabel("UI Theme")
        title.setStyleSheet("font-size: 16px; font-weight: bold;")
        layout.addWidget(title)

        layout.addLayout(self._build_top_bar())

        columns = QHBoxLayout()
        columns.addWidget(self._build_left_column(), 3)
        columns.addWidget(self._build_center_column(), 4)
        columns.addWidget(self._build_right_column(), 3)
        layout.addLayout(columns, 1)

        self.setLayout(layout)

    def _build_top_bar(self) -> QHBoxLayout:
        bar = QHBoxLayout()

        bar.addWidget(QLabel("Name:"))
        self.name_edit = QLineEdit()
        self.name_edit.setMinimumWidth(140)
        self.name_edit.textChanged.connect(self._on_name_changed)
        bar.addWidget(self.name_edit)

        bar.addWidget(QLabel("Extends:"))
        self.extends_combo = QComboBox()
        self.extends_combo.setMinimumWidth(160)
        self.extends_combo.currentIndexChanged.connect(self._on_extends_changed)
        bar.addWidget(self.extends_combo)

        bar.addWidget(QLabel("Palette:"))
        self.palette_combo = QComboBox()
        self.palette_combo.setMinimumWidth(160)
        self.palette_combo.currentIndexChanged.connect(self._on_palette_combo_changed)
        bar.addWidget(self.palette_combo)

        bar.addStretch()

        import_btn = QPushButton("Import")
        import_btn.setProperty("secondary", True)
        import_btn.clicked.connect(self._on_import)
        bar.addWidget(import_btn)

        export_btn = QPushButton("Export")
        export_btn.setProperty("secondary", True)
        export_btn.clicked.connect(self._on_export)
        bar.addWidget(export_btn)

        apply_btn = QPushButton("Apply")
        apply_btn.setProperty("secondary", True)
        apply_btn.clicked.connect(self._on_apply)
        bar.addWidget(apply_btn)

        save_btn = QPushButton("Save")
        save_btn.setProperty("success", True)
        save_btn.clicked.connect(self._on_save)
        bar.addWidget(save_btn)

        close_btn = QPushButton("Close")
        close_btn.setProperty("secondary", True)
        close_btn.clicked.connect(self.reject)
        bar.addWidget(close_btn)

        return bar

    def _build_left_column(self) -> QWidget:
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        container = QWidget()
        col = QVBoxLayout(container)
        col.setContentsMargins(2, 2, 2, 2)

        heading = QLabel("Tokens")
        heading.setStyleSheet("font-weight: bold;")
        col.addWidget(heading)

        palette_group = CollapsibleGroupBox("Palette")
        self.palette_container = QWidget()
        self.palette_layout = QVBoxLayout(self.palette_container)
        self.palette_layout.setContentsMargins(0, 0, 0, 0)
        palette_group.add_widget(self.palette_container)
        col.addWidget(palette_group)

        var_group = CollapsibleGroupBox("Variables")
        self.variables_container = QWidget()
        self.variables_layout = QVBoxLayout(self.variables_container)
        self.variables_layout.setContentsMargins(0, 0, 0, 0)
        var_group.add_widget(self.variables_container)
        col.addWidget(var_group)

        roles_group = CollapsibleGroupBox("Font roles")
        self.roles_container = QWidget()
        self.roles_layout = QVBoxLayout(self.roles_container)
        self.roles_layout.setContentsMargins(0, 0, 0, 0)
        roles_group.add_widget(self.roles_container)
        col.addWidget(roles_group)

        defaults_group = CollapsibleGroupBox("Defaults")
        defaults_group.add_widget(self._build_defaults_ui())
        col.addWidget(defaults_group)

        col.addStretch()
        scroll.setWidget(container)
        return scroll

    def _build_defaults_ui(self) -> QWidget:
        """Build the theme-level Defaults editor (default font / size / base scale)."""
        container = QWidget()
        grid = QGridLayout(container)
        grid.setContentsMargins(0, 0, 0, 0)
        grid.setSpacing(4)

        grid.addWidget(QLabel("Font:"), 0, 0)
        self.default_font_widget = PathPropertyWidget("Font")
        self.default_font_widget.setObjectName("uithemeDefaultFont")
        self.default_font_widget.value_changed.connect(
            lambda _value: self._on_model_changed())
        grid.addWidget(self.default_font_widget, 0, 1)

        grid.addWidget(QLabel("Font size:"), 1, 0)
        self.default_font_size_spin = QDoubleSpinBox()
        self.default_font_size_spin.setObjectName("uithemeDefaultFontSize")
        self.default_font_size_spin.setRange(0.0, 512.0)
        self.default_font_size_spin.setDecimals(1)
        self.default_font_size_spin.valueChanged.connect(
            lambda _value: self._on_model_changed())
        grid.addWidget(self.default_font_size_spin, 1, 1)

        grid.addWidget(QLabel("Base scale:"), 2, 0)
        self.default_base_scale_spin = QDoubleSpinBox()
        self.default_base_scale_spin.setObjectName("uithemeDefaultBaseScale")
        self.default_base_scale_spin.setRange(0.0, 100.0)
        self.default_base_scale_spin.setDecimals(3)
        self.default_base_scale_spin.setSingleStep(0.05)
        self.default_base_scale_spin.setValue(1.0)
        self.default_base_scale_spin.valueChanged.connect(
            lambda _value: self._on_model_changed())
        grid.addWidget(self.default_base_scale_spin, 2, 1)

        grid.setColumnStretch(1, 1)
        return container

    def _refresh_defaults_ui(self) -> None:
        """Load the theme-level default font/size/scale into the Defaults widgets."""
        self.default_font_widget.set_value(str(self.theme.get("default_font", "") or ""))
        try:
            font_size = float(self.theme.get("default_font_size", 0.0) or 0.0)
        except (TypeError, ValueError):
            font_size = 0.0
        self.default_font_size_spin.setValue(font_size)
        try:
            base_scale = float(self.theme.get("default_base_scale", 1.0) or 1.0)
        except (TypeError, ValueError):
            base_scale = 1.0
        self.default_base_scale_spin.setValue(base_scale)

    def _build_center_column(self) -> QWidget:
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        container = QWidget()
        col = QVBoxLayout(container)

        heading = QLabel("Preview")
        heading.setStyleSheet("font-weight: bold;")
        col.addWidget(heading)

        # Prefer the live engine-rendered preview; only fall back to the Qt
        # approximation gallery when the engine path is unavailable.
        if self._try_build_engine_preview(col):
            scroll.setWidget(container)
            return scroll

        self._build_qt_preview(col)
        scroll.setWidget(container)
        return scroll

    def _try_build_engine_preview(self, col: QVBoxLayout) -> bool:
        """Build the embedded engine preview into `col`; return True on success.

        Builds the detached preview scene first so a failure leaves nothing
        attached to the layout (allowing the Qt gallery to take over cleanly).
        """
        if not (self.editor_session and self.editor_bridge and _VIEWPORT_AVAILABLE):
            return False
        try:
            self._build_preview_scene()
            # Minimal toolbar (gizmo move/scale/rotate/all only) + forced 2D so
            # middle-drag panning and wheel zoom use the 2D camera immediately.
            viewport = ViewportWidget("UIThemePreview", self,
                                      toolbar_mode="minimal", force_2d=True)
            viewport.setMinimumHeight(360)
            self._preview_viewport = viewport
            col.addWidget(viewport, 1)

            # Add Control: a menu of every UI component the user can drop into the
            # preview. Remove Selected deletes whatever control is picked.
            add_btn = QPushButton("+ Add Control")
            add_btn.setProperty("success", True)
            add_btn.setMenu(self._build_add_menu())
            viewport.add_toolbar_widget(add_btn)

            remove_btn = QPushButton("Remove Selected")
            remove_btn.setProperty("danger", True)
            remove_btn.clicked.connect(self._on_remove_selected_control)
            viewport.add_toolbar_widget(remove_btn)

            # Persist the layout whenever a control is dragged; track selection so
            # Remove Selected knows which control to delete.
            viewport.gizmo_drag_ended.connect(self._save_preview_layout)
            viewport.node_selected.connect(self._on_preview_node_selected)

            note = QLabel("Engine preview — click a control to select, drag the gizmo "
                          "to position it, middle-drag to pan, wheel to zoom. Layout "
                          "is saved to this project automatically.")
            note.setWordWrap(True)
            note.setProperty("secondary", True)
            col.addWidget(note)

            self._use_engine_preview = True
            # Gizmo drags record undo commands and dirty the active scene; suppress
            # that while this modal preview drives its detached scene.
            self._set_undo_recording(False)
            # Defer rendering init until after the dialog is shown so the native
            # window handle (winId) is valid.
            QTimer.singleShot(0, self._start_preview)
            return True
        except Exception as exc:
            import traceback
            print(f"UI Theme: engine preview unavailable, using Qt gallery: {exc}")
            traceback.print_exc()
            if self._preview_viewport is not None:
                try:
                    self._preview_viewport.setParent(None)
                except Exception:
                    pass
            self._preview_viewport = None
            self._preview_scene = None
            self._preview_controls = []
            self._use_engine_preview = False
            self._set_undo_recording(True)
            return False

    def _build_qt_preview(self, col: QVBoxLayout) -> None:
        """Build the fallback Qt approximation gallery into `col`."""
        note = QLabel("Approximate preview - the scene viewport shows the live engine result.")
        note.setWordWrap(True)
        note.setProperty("secondary", True)
        col.addWidget(note)

        controls = QHBoxLayout()
        self.state_label = QLabel("Showing: Normal + Disabled")
        self.state_label.setProperty("secondary", True)
        controls.addWidget(self.state_label)
        controls.addStretch()
        self.pv_bg_dark = QCheckBox("Dark background")
        self.pv_bg_dark.setChecked(True)
        self.pv_bg_dark.toggled.connect(lambda _checked: self._refresh_preview())
        controls.addWidget(self.pv_bg_dark)
        col.addLayout(controls)

        self.preview_frame = QFrame()
        self.preview_frame.setObjectName("uithemePreviewFrame")
        self.preview_frame.setMinimumHeight(360)
        gallery = QVBoxLayout(self.preview_frame)
        gallery.setContentsMargins(16, 16, 16, 16)
        gallery.setSpacing(12)

        self.pv_label = QLabel("The quick brown fox - Label")
        gallery.addWidget(self.pv_label)

        btn_row = QHBoxLayout()
        self.pv_button = QPushButton("Button")
        self.pv_button_disabled = QPushButton("Disabled")
        self.pv_button_disabled.setEnabled(False)
        btn_row.addWidget(self.pv_button)
        btn_row.addWidget(self.pv_button_disabled)
        btn_row.addStretch()
        gallery.addLayout(btn_row)

        self.pv_lineedit = QLineEdit("Editable text")
        gallery.addWidget(self.pv_lineedit)

        self.pv_checkbox = QCheckBox("Checkbox")
        self.pv_checkbox.setChecked(True)
        gallery.addWidget(self.pv_checkbox)

        self.pv_panel = QFrame()
        self.pv_panel.setObjectName("uithemePreviewPanel")
        self.pv_panel.setMinimumHeight(70)
        panel_layout = QVBoxLayout(self.pv_panel)
        panel_layout.addWidget(QLabel("Panel"))
        gallery.addWidget(self.pv_panel)

        gallery.addStretch()
        col.addWidget(self.preview_frame, 1)

    def _build_right_column(self) -> QWidget:
        wrapper = QWidget()
        col = QVBoxLayout(wrapper)
        col.setContentsMargins(2, 2, 2, 2)

        heading = QLabel("Selected Type")
        heading.setStyleSheet("font-weight: bold;")
        col.addWidget(heading)

        type_row = QHBoxLayout()
        type_row.addWidget(QLabel("Type:"))
        self.type_combo = QComboBox()
        self.type_combo.setObjectName("uithemeTypeCombo")
        self.type_combo.currentIndexChanged.connect(lambda _index: self._rebuild_type_editor())
        type_row.addWidget(self.type_combo, 1)

        new_variation_btn = QPushButton("+ Variation")
        new_variation_btn.setObjectName("uithemeNewVariationButton")
        new_variation_btn.setProperty("success", True)
        new_variation_btn.clicked.connect(self._on_new_variation)
        type_row.addWidget(new_variation_btn)

        self.delete_variation_btn = QPushButton("Delete")
        self.delete_variation_btn.setObjectName("uithemeDeleteVariationButton")
        self.delete_variation_btn.setProperty("danger", True)
        self.delete_variation_btn.clicked.connect(self._on_delete_variation)
        type_row.addWidget(self.delete_variation_btn)

        col.addLayout(type_row)
        self._populate_type_combo()

        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        self.type_editor_container = QWidget()
        self.type_editor_layout = QVBoxLayout(self.type_editor_container)
        self.type_editor_layout.setContentsMargins(0, 0, 0, 0)
        scroll.setWidget(self.type_editor_container)
        col.addWidget(scroll, 1)

        return wrapper

    # -------------------------------------------------------------- paths

    def _res_to_abs(self, res_path: str) -> str:
        """Resolve a res:// path (or pass through an absolute path)."""
        if not res_path:
            return ""
        if res_path.startswith("res://"):
            rel = res_path[len("res://"):]
            if self.project_dir:
                return os.path.normpath(os.path.join(self.project_dir, rel))
            return os.path.normpath(rel)
        return res_path

    def _abs_to_res(self, abs_path: str) -> str:
        """Convert an absolute path to a res:// path, falling back to manual mapping."""
        if not abs_path:
            return ""
        res = convert_to_res_path(abs_path)
        if res and res.startswith("res://"):
            return res
        if self.project_dir:
            try:
                rel = os.path.relpath(abs_path, self.project_dir)
                if not rel.startswith(".."):
                    return "res://" + rel.replace("\\", "/")
            except ValueError:
                pass
        return abs_path

    def _find_theme_files(self, suffix: str) -> List[Path]:
        results: List[Path] = []
        if not self.project_dir:
            return results
        try:
            for path in sorted(Path(self.project_dir).rglob("*" + suffix)):
                if path.is_file():
                    results.append(path)
        except Exception as exc:
            print(f"UI Theme: failed to scan for {suffix}: {exc}")
        return results

    def _find_first_theme(self) -> Optional[str]:
        files = self._find_theme_files(".uitheme")
        return str(files[0]) if files else None

    # ------------------------------------------------------------- loading

    def _new_theme(self) -> None:
        self.theme = {
            "lupine_theme": 1,
            "name": "Default",
            "extends": "",
            "variables": {},
            "font_roles": {},
            "types": {},
        }
        self.palette_slots = []
        self.palette_mode = "inline"
        self.palette_ext_res = ""
        self.palette_name = "Palette"
        self.theme_path = ""
        self._variables = []
        self._font_roles = []

    def _load_theme(self, abs_path: str) -> None:
        data: Any = None
        try:
            with open(abs_path, "r", encoding="utf-8") as fh:
                data = json.load(fh)
        except Exception as exc:
            QMessageBox.warning(self, "UI Theme",
                                f"Could not load theme:\n{abs_path}\n\n{exc}")
            self._new_theme()
            return
        if not isinstance(data, dict):
            QMessageBox.warning(self, "UI Theme",
                                f"Theme file is not a JSON object:\n{abs_path}")
            self._new_theme()
            return

        self.theme = data
        self.theme_path = abs_path
        self.theme.setdefault("lupine_theme", 1)
        self.theme.setdefault("name", Path(abs_path).stem)
        self.theme.setdefault("extends", "")
        if not isinstance(self.theme.get("types"), dict):
            self.theme["types"] = {}

        self._variables = []
        raw_vars = self.theme.get("variables")
        if isinstance(raw_vars, dict):
            for key, value in raw_vars.items():
                try:
                    num = float(value)
                except (TypeError, ValueError):
                    num = 0.0
                self._variables.append({"key": str(key), "value": num})

        self._font_roles = []
        raw_roles = self.theme.get("font_roles")
        if isinstance(raw_roles, dict):
            for role, info in raw_roles.items():
                info = info if isinstance(info, dict) else {}
                try:
                    size = float(info.get("size", 16) or 16)
                except (TypeError, ValueError):
                    size = 16.0
                self._font_roles.append({
                    "role": str(role),
                    "font": str(info.get("font", "")),
                    "size": size,
                })

        self._load_palette()

    def _load_palette(self) -> None:
        pal = self.theme.get("palette", None)
        if isinstance(pal, str) and pal.strip():
            self.palette_mode = "external"
            self.palette_ext_res = pal.strip()
            self._load_external_palette(self.palette_ext_res)
        elif isinstance(pal, dict):
            self.palette_mode = "inline"
            self.palette_ext_res = ""
            self.palette_name = str(pal.get("name", "Palette"))
            self.palette_slots = self._normalize_slots(pal.get("slots", []))
        else:
            # Absent -> treat as an empty inline palette the user can fill in.
            self.palette_mode = "inline"
            self.palette_ext_res = ""
            self.palette_name = "Palette"
            self.palette_slots = []

    def _load_external_palette(self, res_path: str) -> None:
        abs_path = self._res_to_abs(res_path)
        data: Any = {}
        try:
            with open(abs_path, "r", encoding="utf-8") as fh:
                data = json.load(fh)
        except Exception as exc:
            print(f"UI Theme: failed to load palette {abs_path}: {exc}")
            data = {}
        if not isinstance(data, dict):
            data = {}
        self.palette_name = str(data.get("name", Path(abs_path).stem))
        self.palette_slots = self._normalize_slots(data.get("slots", []))

    def _normalize_slots(self, slots: Any) -> List[Dict[str, Any]]:
        out: List[Dict[str, Any]] = []
        if not isinstance(slots, list):
            return out
        for slot in slots:
            if not isinstance(slot, dict):
                continue
            color = slot.get("color", [1.0, 1.0, 1.0, 1.0])
            out.append({
                "key": str(slot.get("key", "")),
                "name": str(slot.get("name", "")),
                "color": self._coerce_rgba(color),
            })
        return out

    def _coerce_rgba(self, color: Any, default: Optional[List[float]] = None) -> List[float]:
        if default is None:
            default = [1.0, 1.0, 1.0, 1.0]
        try:
            seq = list(color)
            r = float(seq[0])
            g = float(seq[1])
            b = float(seq[2])
            a = float(seq[3]) if len(seq) > 3 else 1.0
            return [r, g, b, a]
        except (TypeError, ValueError, IndexError):
            return list(default)

    # --------------------------------------------------------- model access

    def _types(self) -> Dict[str, Any]:
        types = self.theme.get("types")
        if not isinstance(types, dict):
            types = {}
            self.theme["types"] = types
        return types

    def _type_dict(self, type_name: str, create: bool = False) -> Optional[Dict[str, Any]]:
        types = self._types()
        td = types.get(type_name)
        if not isinstance(td, dict):
            if not create:
                return None
            td = {}
            types[type_name] = td
        return td

    def _category_dict(self, type_name: str, category: str,
                       create: bool = False) -> Optional[Dict[str, Any]]:
        td = self._type_dict(type_name, create=create)
        if td is None:
            return None
        cat = td.get(category)
        if not isinstance(cat, dict):
            if not create:
                return None
            cat = {}
            td[category] = cat
        return cat

    def _get_entry_value(self, type_name: str, category: str, entry: str) -> Optional[Any]:
        cat = self._category_dict(type_name, category)
        if cat is None:
            return None
        return cat.get(entry)

    def _set_entry_value(self, type_name: str, category: str, entry: str,
                         value: Optional[Dict[str, Any]]) -> None:
        if value is None:
            cat = self._category_dict(type_name, category)
            if cat is not None and entry in cat:
                del cat[entry]
                if not cat:
                    td = self._type_dict(type_name)
                    if td is not None and category in td and not td[category]:
                        del td[category]
            return
        cat = self._category_dict(type_name, category, create=True)
        if cat is not None:
            cat[entry] = value

    def _variable_value(self, key: str, default: float = 0.0) -> float:
        for var in self._variables:
            if var.get("key") == key:
                try:
                    return float(var.get("value", default))
                except (TypeError, ValueError):
                    return default
        return default

    def _variable_keys(self) -> List[str]:
        return [var["key"] for var in self._variables if var.get("key")]

    def _font_role_names(self) -> List[str]:
        return [role["role"] for role in self._font_roles if role.get("role")]

    def _palette_slot(self, key: str) -> Optional[Dict[str, Any]]:
        for slot in self.palette_slots:
            if slot.get("key") == key:
                return slot
        return None

    def _palette_keys(self) -> List[str]:
        return [slot["key"] for slot in self.palette_slots if slot.get("key")]

    # ------------------------------------------------------------ resolution

    def apply_color_op(self, rgba: List[float], op: str, amount: float) -> List[float]:
        """Apply a derived-binding modifier to an rgba color (0..1 floats)."""
        values = self._coerce_rgba(rgba)
        r, g, b, a = values
        try:
            amt = float(amount)
        except (TypeError, ValueError):
            amt = 0.0
        if op == "lighten":
            r = r + (1.0 - r) * amt
            g = g + (1.0 - g) * amt
            b = b + (1.0 - b) * amt
        elif op == "darken":
            r = r * (1.0 - amt)
            g = g * (1.0 - amt)
            b = b * (1.0 - amt)
        elif op == "alpha":
            a = amt
        elif op in ("saturate", "desaturate"):
            col = QColor.fromRgbF(_clamp01(r), _clamp01(g), _clamp01(b), _clamp01(a))
            h, s, v, alpha = col.getHsvF()
            if h < 0:
                h = 0.0
            if op == "saturate":
                s = min(1.0, s + amt)
            else:
                s = max(0.0, s - amt)
            col = QColor.fromHsvF(h, _clamp01(s), v, alpha)
            r, g, b = col.redF(), col.greenF(), col.blueF()
        return [_clamp01(r), _clamp01(g), _clamp01(b), _clamp01(a)]

    def _lookup_color_val(self, type_name: str, entry: str,
                          seen: Optional[set] = None) -> Optional[Dict[str, Any]]:
        if seen is None:
            seen = set()
        if not type_name or type_name in seen:
            return None
        seen.add(type_name)
        td = self._type_dict(type_name)
        if isinstance(td, dict):
            cat = td.get("colors")
            if isinstance(cat, dict) and entry in cat:
                return cat[entry]
            base = td.get("extends", "")
            if base:
                return self._lookup_color_val(str(base), entry, seen)
        return None

    def _lookup_scalar_val(self, type_name: str, entry: str,
                           seen: Optional[set] = None) -> Optional[Dict[str, Any]]:
        if seen is None:
            seen = set()
        if not type_name or type_name in seen:
            return None
        seen.add(type_name)
        td = self._type_dict(type_name)
        if isinstance(td, dict):
            cat = td.get("constants")
            if isinstance(cat, dict) and entry in cat:
                return cat[entry]
            base = td.get("extends", "")
            if base:
                return self._lookup_scalar_val(str(base), entry, seen)
        return None

    def resolve_color(self, type_name: str, entry: str,
                      default: List[float]) -> List[float]:
        """Resolve a type+entry color to an rgba list, following binds/ops/extends."""
        val = self._lookup_color_val(type_name, entry)
        if not isinstance(val, dict):
            return list(default)
        if "value" in val:
            return self._coerce_rgba(val.get("value"), list(default))
        bind = val.get("bind", "")
        if isinstance(bind, str) and bind.startswith("palette:"):
            slot = self._palette_slot(bind.split(":", 1)[1])
            if slot is not None:
                rgba = self._coerce_rgba(slot.get("color"), list(default))
                op = val.get("op")
                if op and op != "none":
                    rgba = self.apply_color_op(rgba, str(op), val.get("amount", 0.0))
                return rgba
        return list(default)

    def resolve_constant(self, type_name: str, entry: str, default: float) -> float:
        """Resolve a type+entry scalar, following var binds and type extends."""
        val = self._lookup_scalar_val(type_name, entry)
        if not isinstance(val, dict):
            return float(default)
        if "value" in val:
            try:
                return float(val.get("value"))
            except (TypeError, ValueError):
                return float(default)
        bind = val.get("bind", "")
        if isinstance(bind, str) and bind.startswith("var:"):
            return self._variable_value(bind.split(":", 1)[1], float(default))
        return float(default)

    def _rgba_to_hex(self, rgba: List[float]) -> str:
        values = self._coerce_rgba(rgba)
        return "#{:02x}{:02x}{:02x}".format(_to_255(values[0]), _to_255(values[1]),
                                            _to_255(values[2]))

    def _hex_to_rgba(self, hex_str: str, alpha: float = 1.0) -> List[float]:
        col = QColor(hex_str)
        if not col.isValid():
            col = QColor("#000000")
        return [col.redF(), col.greenF(), col.blueF(), float(alpha)]

    # --------------------------------------------------------- top-bar refresh

    def _refresh_all(self) -> None:
        self._loading = True
        self.name_edit.setText(str(self.theme.get("name", "Default")))
        self._populate_extends_combo()
        self._populate_palette_combo()
        self._rebuild_palette_ui()
        self._rebuild_variables_ui()
        self._rebuild_font_roles_ui()
        self._refresh_defaults_ui()
        self._populate_type_combo()
        self._rebuild_type_editor()
        self._refresh_preview()
        self._rebuild_preview_scene_theme()
        self._loading = False

    def _populate_extends_combo(self) -> None:
        self.extends_combo.blockSignals(True)
        self.extends_combo.clear()
        self.extends_combo.addItem("(none)", "")
        current = self.theme.get("extends", "") or ""
        self_res = self._abs_to_res(self.theme_path) if self.theme_path else ""
        for path in self._find_theme_files(".uitheme"):
            res = self._abs_to_res(str(path))
            if res == self_res:
                continue
            self.extends_combo.addItem(path.name, res)
        if current:
            idx = self.extends_combo.findData(current)
            if idx < 0:
                self.extends_combo.addItem(current, current)
                idx = self.extends_combo.findData(current)
            self.extends_combo.setCurrentIndex(idx)
        else:
            self.extends_combo.setCurrentIndex(0)
        self.extends_combo.blockSignals(False)

    def _populate_palette_combo(self) -> None:
        self.palette_combo.blockSignals(True)
        self.palette_combo.clear()
        self.palette_combo.addItem("(none)", "none")
        self.palette_combo.addItem("(inline)", "inline")
        for path in self._find_theme_files(".palette"):
            self.palette_combo.addItem(path.name, self._abs_to_res(str(path)))
        if self.palette_mode == "external" and self.palette_ext_res:
            idx = self.palette_combo.findData(self.palette_ext_res)
            if idx < 0:
                self.palette_combo.addItem(self.palette_ext_res, self.palette_ext_res)
                idx = self.palette_combo.findData(self.palette_ext_res)
            self.palette_combo.setCurrentIndex(idx)
        elif self.palette_mode == "none":
            self.palette_combo.setCurrentIndex(0)
        else:
            self.palette_combo.setCurrentIndex(1)
        self.palette_combo.blockSignals(False)

    def _on_name_changed(self, text: str) -> None:
        if self._loading:
            return
        self.theme["name"] = text

    def _on_extends_changed(self, index: int) -> None:
        if self._loading:
            return
        data = self.extends_combo.itemData(index)
        self.theme["extends"] = data if data else ""
        self._refresh_preview()

    def _on_palette_combo_changed(self, index: int) -> None:
        if self._loading:
            return
        data = self.palette_combo.itemData(index)
        if data == "none":
            self.palette_mode = "none"
        elif data == "inline":
            self.palette_mode = "inline"
        else:
            self.palette_mode = "external"
            self.palette_ext_res = str(data)
            self._load_external_palette(self.palette_ext_res)
        self._rebuild_palette_ui()
        self._rebuild_type_editor()
        self._refresh_preview()

    # ------------------------------------------------------------ left tokens

    def _clear_layout(self, layout) -> None:
        while layout.count():
            item = layout.takeAt(0)
            widget = item.widget()
            if widget is not None:
                widget.setParent(None)
            else:
                child = item.layout()
                if child is not None:
                    self._clear_layout(child)

    def _rebuild_palette_ui(self) -> None:
        self._clear_layout(self.palette_layout)

        if self.palette_mode == "none":
            label = QLabel("No palette - this theme uses literal colors only.")
            label.setWordWrap(True)
            label.setProperty("secondary", True)
            self.palette_layout.addWidget(label)
            return

        if self.palette_mode == "external":
            src = QLabel(f"External: {self.palette_ext_res}")
            src.setWordWrap(True)
            src.setProperty("secondary", True)
            self.palette_layout.addWidget(src)

        grid = QGridLayout()
        grid.setSpacing(4)
        for row, slot in enumerate(self.palette_slots):
            swatch = ColorButton(color=self._rgba_to_hex(slot["color"]))
            swatch.color_changed.connect(
                lambda hex_value, s=slot: self._on_palette_color_changed(s, hex_value))

            key_edit = QLineEdit(slot.get("key", ""))
            key_edit.setPlaceholderText("key")
            key_edit.editingFinished.connect(
                lambda e=key_edit, s=slot: self._on_palette_key_changed(s, e.text()))

            name_edit = QLineEdit(slot.get("name", ""))
            name_edit.setPlaceholderText("name")
            name_edit.editingFinished.connect(
                lambda e=name_edit, s=slot: self._on_palette_name_changed(s, e.text()))

            find_btn = QPushButton("Find usages")
            find_btn.setProperty("secondary", True)
            find_btn.clicked.connect(
                lambda _checked, s=slot: self._on_find_usages(s.get("key", "")))

            remove_btn = QPushButton("−")
            remove_btn.setProperty("danger", True)
            remove_btn.setFixedWidth(28)
            remove_btn.clicked.connect(
                lambda _checked, s=slot: self._on_remove_palette_slot(s))

            grid.addWidget(swatch, row, 0)
            grid.addWidget(key_edit, row, 1)
            grid.addWidget(name_edit, row, 2)
            grid.addWidget(find_btn, row, 3)
            grid.addWidget(remove_btn, row, 4)
        self.palette_layout.addLayout(grid)

        add_btn = QPushButton("+ Add color")
        add_btn.setProperty("success", True)
        add_btn.clicked.connect(self._on_add_palette_slot)
        self.palette_layout.addWidget(add_btn)

    def _on_palette_color_changed(self, slot: Dict[str, Any], hex_value: str) -> None:
        existing = slot.get("color", [0.0, 0.0, 0.0, 1.0])
        alpha = existing[3] if isinstance(existing, list) and len(existing) > 3 else 1.0
        slot["color"] = self._hex_to_rgba(hex_value, alpha)
        self._on_model_changed()

    def _on_palette_key_changed(self, slot: Dict[str, Any], text: str) -> None:
        slot["key"] = text.strip()
        self._rebuild_type_editor()
        self._on_model_changed()

    def _on_palette_name_changed(self, slot: Dict[str, Any], text: str) -> None:
        slot["name"] = text

    def _on_add_palette_slot(self) -> None:
        existing = set(self._palette_keys())
        key = "color"
        n = 1
        while key in existing:
            key = f"color_{n}"
            n += 1
        self.palette_slots.append({"key": key, "name": "", "color": [1.0, 1.0, 1.0, 1.0]})
        self._rebuild_palette_ui()
        self._rebuild_type_editor()
        self._on_model_changed()

    def _on_remove_palette_slot(self, slot: Dict[str, Any]) -> None:
        try:
            self.palette_slots.remove(slot)
        except ValueError:
            pass
        self._rebuild_palette_ui()
        self._rebuild_type_editor()
        self._on_model_changed()

    def _on_find_usages(self, key: str) -> None:
        if not key:
            QMessageBox.information(self, "Find usages", "This slot has no key yet.")
            return
        target = f"palette:{key}"
        hits: List[str] = []
        for type_name, type_data in self._types().items():
            if not isinstance(type_data, dict):
                continue
            colors = type_data.get("colors")
            if not isinstance(colors, dict):
                continue
            for entry, val in colors.items():
                if isinstance(val, dict) and val.get("bind") == target:
                    op = val.get("op")
                    hits.append(f"{type_name}.{entry}" + (f"  ({op})" if op and op != "none" else ""))
        if hits:
            QMessageBox.information(
                self, "Find usages",
                f"'{key}' is used by {len(hits)} entr{'y' if len(hits) == 1 else 'ies'}:\n\n"
                + "\n".join(hits))
        else:
            QMessageBox.information(self, "Find usages",
                                   f"'{key}' is not bound by any type entry yet.")

    def _rebuild_variables_ui(self) -> None:
        self._clear_layout(self.variables_layout)
        for var in self._variables:
            row = QWidget()
            row_layout = QHBoxLayout(row)
            row_layout.setContentsMargins(0, 0, 0, 0)

            name_edit = QLineEdit(var.get("key", ""))
            name_edit.setPlaceholderText("name")
            name_edit.editingFinished.connect(
                lambda e=name_edit, v=var: self._on_variable_key_changed(v, e.text()))

            spin = QDoubleSpinBox()
            spin.setRange(-1000000.0, 1000000.0)
            spin.setDecimals(3)
            spin.setValue(float(var.get("value", 0.0)))
            spin.valueChanged.connect(
                lambda value, v=var: self._on_variable_value_changed(v, value))

            remove_btn = QPushButton("−")
            remove_btn.setProperty("danger", True)
            remove_btn.setFixedWidth(28)
            remove_btn.clicked.connect(lambda _checked, v=var: self._on_remove_variable(v))

            row_layout.addWidget(name_edit, 2)
            row_layout.addWidget(spin, 1)
            row_layout.addWidget(remove_btn)
            self.variables_layout.addWidget(row)

        add_btn = QPushButton("+ Add variable")
        add_btn.setProperty("success", True)
        add_btn.clicked.connect(self._on_add_variable)
        self.variables_layout.addWidget(add_btn)

    def _on_variable_key_changed(self, var: Dict[str, Any], text: str) -> None:
        var["key"] = text.strip()
        self._rebuild_type_editor()
        self._on_model_changed()

    def _on_variable_value_changed(self, var: Dict[str, Any], value: float) -> None:
        var["value"] = float(value)
        self._on_model_changed()

    def _on_add_variable(self) -> None:
        existing = set(self._variable_keys())
        key = "variable"
        n = 1
        while key in existing:
            key = f"variable_{n}"
            n += 1
        self._variables.append({"key": key, "value": 0.0})
        self._rebuild_variables_ui()
        self._rebuild_type_editor()
        self._on_model_changed()

    def _on_remove_variable(self, var: Dict[str, Any]) -> None:
        try:
            self._variables.remove(var)
        except ValueError:
            pass
        self._rebuild_variables_ui()
        self._rebuild_type_editor()
        self._on_model_changed()

    def _rebuild_font_roles_ui(self) -> None:
        self._clear_layout(self.roles_layout)
        for role in self._font_roles:
            row = QWidget()
            row_layout = QHBoxLayout(row)
            row_layout.setContentsMargins(0, 0, 0, 0)

            name_edit = QLineEdit(role.get("role", ""))
            name_edit.setPlaceholderText("role name")
            name_edit.setFixedWidth(90)
            name_edit.editingFinished.connect(
                lambda e=name_edit, r=role: self._on_role_name_changed(r, e.text()))

            path_widget = PathPropertyWidget("Font")
            path_widget.set_value(role.get("font", ""))
            path_widget.value_changed.connect(
                lambda value, r=role: self._on_role_font_changed(r, value))

            size_spin = QDoubleSpinBox()
            size_spin.setRange(1.0, 512.0)
            size_spin.setDecimals(1)
            size_spin.setValue(float(role.get("size", 16.0)))
            size_spin.valueChanged.connect(
                lambda value, r=role: self._on_role_size_changed(r, value))

            remove_btn = QPushButton("−")
            remove_btn.setProperty("danger", True)
            remove_btn.setFixedWidth(28)
            remove_btn.clicked.connect(lambda _checked, r=role: self._on_remove_role(r))

            row_layout.addWidget(name_edit)
            row_layout.addWidget(path_widget, 1)
            row_layout.addWidget(size_spin)
            row_layout.addWidget(remove_btn)
            self.roles_layout.addWidget(row)

        add_btn = QPushButton("+ Add role")
        add_btn.setProperty("success", True)
        add_btn.clicked.connect(self._on_add_role)
        self.roles_layout.addWidget(add_btn)

    def _on_role_name_changed(self, role: Dict[str, Any], text: str) -> None:
        role["role"] = text.strip()
        self._rebuild_type_editor()
        self._on_model_changed()

    def _on_role_font_changed(self, role: Dict[str, Any], value: str) -> None:
        role["font"] = value
        self._on_model_changed()

    def _on_role_size_changed(self, role: Dict[str, Any], value: float) -> None:
        role["size"] = float(value)
        self._on_model_changed()

    def _on_add_role(self) -> None:
        existing = set(self._font_role_names())
        name = "body"
        n = 1
        while name in existing:
            name = f"role_{n}"
            n += 1
        self._font_roles.append({"role": name, "font": "", "size": 16.0})
        self._rebuild_font_roles_ui()
        self._rebuild_type_editor()
        self._on_model_changed()

    def _on_remove_role(self, role: Dict[str, Any]) -> None:
        try:
            self._font_roles.remove(role)
        except ValueError:
            pass
        self._rebuild_font_roles_ui()
        self._rebuild_type_editor()
        self._on_model_changed()

    # ----------------------------------------------------------- right panel

    def _all_type_names(self) -> List[str]:
        """Ordered base type list: the base catalog plus any state-only types."""
        names: List[str] = list(TYPE_CATALOG.keys())
        for name in STATE_CATALOG.keys():
            if name not in names:
                names.append(name)
        return names

    def _is_base_type(self, type_name: str) -> bool:
        """True when `type_name` is a built-in base type (catalog or state-only)."""
        return type_name in TYPE_CATALOG or type_name in STATE_CATALOG

    def _variation_names(self) -> List[str]:
        """Ordered names of user-defined variations (type entries that aren't base types)."""
        names: List[str] = []
        for name in self._types().keys():
            if not self._is_base_type(str(name)):
                names.append(str(name))
        return names

    def _variation_base(self, type_name: str) -> str:
        """The base type a variation extends, or "" when unset."""
        td = self._type_dict(type_name)
        if isinstance(td, dict):
            return str(td.get("extends", "") or "")
        return ""

    def _current_type_name(self) -> str:
        """The actual type name backing the current type-combo selection."""
        data = self.type_combo.currentData()
        if data:
            return str(data)
        return self.type_combo.currentText()

    def _resolve_catalog_type(self, type_name: str,
                              seen: Optional[set] = None) -> Optional[str]:
        """Follow `extends` until a TYPE_CATALOG base is reached (entry source)."""
        if seen is None:
            seen = set()
        if not type_name or type_name in seen:
            return None
        seen.add(type_name)
        if type_name in TYPE_CATALOG:
            return type_name
        base = self._variation_base(type_name)
        if base:
            return self._resolve_catalog_type(base, seen)
        return None

    def _resolve_state_type(self, type_name: str,
                            seen: Optional[set] = None) -> Optional[str]:
        """Follow `extends` until a STATE_CATALOG base is reached (state source)."""
        if seen is None:
            seen = set()
        if not type_name or type_name in seen:
            return None
        seen.add(type_name)
        if type_name in STATE_CATALOG:
            return type_name
        base = self._variation_base(type_name)
        if base:
            return self._resolve_state_type(base, seen)
        return None

    def _populate_type_combo(self) -> None:
        """Fill the type selector with base types, then a separated variations group."""
        if not hasattr(self, "type_combo"):
            return
        prev = self._current_type_name()
        self.type_combo.blockSignals(True)
        self.type_combo.clear()
        for type_name in self._all_type_names():
            self.type_combo.addItem(type_name, type_name)
        variations = self._variation_names()
        if variations:
            self.type_combo.insertSeparator(self.type_combo.count())
            for name in variations:
                base = self._variation_base(name)
                label = f"{name}  ({base})" if base else f"{name}  (variation)"
                self.type_combo.addItem(label, name)
        idx = self.type_combo.findData(prev) if prev else -1
        self.type_combo.setCurrentIndex(idx if idx >= 0 else 0)
        self.type_combo.blockSignals(False)

    def _on_new_variation(self) -> None:
        """Prompt for a variation name and base type, then create the type entry."""
        existing = set(self._types().keys()) | set(self._all_type_names())
        name, ok = QInputDialog.getText(self, "New Variation",
                                        "Variation name (e.g. PrimaryButton):")
        if not ok:
            return
        name = name.strip()
        if not name:
            QMessageBox.information(self, "New Variation", "A variation needs a name.")
            return
        if name in existing:
            QMessageBox.warning(self, "New Variation",
                                f"'{name}' already exists as a type or variation.")
            return
        bases = self._all_type_names() + self._variation_names()
        current = self._current_type_name()
        start = bases.index(current) if current in bases else 0
        base, ok = QInputDialog.getItem(self, "New Variation",
                                        "Base type it varies:", bases, start, False)
        if not ok:
            return
        self._types()[name] = {"extends": str(base).strip()}
        self._populate_type_combo()
        idx = self.type_combo.findData(name)
        if idx >= 0:
            self.type_combo.setCurrentIndex(idx)
        self._rebuild_type_editor()
        self._on_model_changed()

    def _on_delete_variation(self) -> None:
        """Delete the currently selected variation (base types cannot be deleted)."""
        type_name = self._current_type_name()
        if not type_name or self._is_base_type(type_name):
            QMessageBox.information(self, "Delete Variation",
                                    "Only variations can be deleted. Select a variation first.")
            return
        confirm = QMessageBox.question(
            self, "Delete Variation",
            f"Delete the variation '{type_name}' and all of its overrides?")
        if confirm != QMessageBox.StandardButton.Yes:
            return
        self._types().pop(type_name, None)
        self._populate_type_combo()
        self._rebuild_type_editor()
        self._on_model_changed()

    def _rebuild_type_editor(self) -> None:
        self._clear_layout(self.type_editor_layout)
        type_name = self._current_type_name()
        is_variation = bool(type_name) and not self._is_base_type(type_name)
        if hasattr(self, "delete_variation_btn"):
            self.delete_variation_btn.setEnabled(is_variation)
        if not type_name:
            return
        if not self._is_base_type(type_name) and type_name not in self._types():
            return

        ext_row = QWidget()
        ext_layout = QHBoxLayout(ext_row)
        ext_layout.setContentsMargins(0, 0, 0, 0)
        ext_layout.addWidget(QLabel("Type extends:"))
        ext_combo = QComboBox()
        ext_combo.addItem("(none)")
        for other in self._all_type_names() + self._variation_names():
            if other != type_name:
                ext_combo.addItem(other)
        td = self._type_dict(type_name)
        current_ext = str(td.get("extends", "")) if isinstance(td, dict) else ""
        if current_ext:
            idx = ext_combo.findText(current_ext)
            if idx < 0:
                ext_combo.addItem(current_ext)
                idx = ext_combo.findText(current_ext)
            ext_combo.setCurrentIndex(idx)
        else:
            ext_combo.setCurrentIndex(0)
        ext_combo.currentTextChanged.connect(
            lambda text, t=type_name: self._on_type_extends_changed(t, text))
        ext_layout.addWidget(ext_combo, 1)
        self.type_editor_layout.addWidget(ext_row)

        catalog_type = self._resolve_catalog_type(type_name)
        catalog = TYPE_CATALOG.get(catalog_type, {}) if catalog_type else {}
        for kind in ("color", "constant", "font", "image"):
            entries = [entry for entry, entry_kind in catalog.items() if entry_kind == kind]
            if not entries:
                continue
            group = CollapsibleGroupBox(KIND_GROUP_TITLE[kind])
            for entry in entries:
                if kind == "color":
                    group.add_widget(self._build_color_row(type_name, entry))
                elif kind == "constant":
                    group.add_widget(self._build_constant_row(type_name, entry))
                elif kind == "font":
                    group.add_widget(self._build_font_row(type_name, entry))
                else:
                    group.add_widget(self._build_image_row(type_name, entry))
            self.type_editor_layout.addWidget(group)

        self._build_states_section(type_name)
        self._build_styleboxes_section(type_name)

        self.type_editor_layout.addStretch()

    def _build_states_section(self, type_name: str) -> None:
        """Append the per-state authoring section (modulation + optional tween)."""
        state_type = self._resolve_state_type(type_name)
        state_info = STATE_CATALOG.get(state_type) if state_type else None
        if not state_info:
            return
        has_tween = bool(state_info.get("tween", False))
        states_group = CollapsibleGroupBox("States")
        for prefix in state_info["states"]:
            state_box = CollapsibleGroupBox(_humanize_camel(prefix))
            state_box.add_widget(self._build_modulation_row(
                type_name, prefix + "Modulation", label="Modulation"))
            if has_tween:
                for suffix, kind in STATE_TWEEN_ENTRIES:
                    entry = prefix + suffix
                    entry_label = _humanize_camel(suffix)
                    if kind == "bool":
                        state_box.add_widget(self._build_bool_row(type_name, entry, label=entry_label))
                    elif kind == "vec2":
                        state_box.add_widget(self._build_vec2_row(type_name, entry, label=entry_label))
                    else:
                        state_box.add_widget(self._build_constant_row(type_name, entry, label=entry_label))
            states_group.add_widget(state_box)
        self.type_editor_layout.addWidget(states_group)

    def _on_type_extends_changed(self, type_name: str, text: str) -> None:
        if self._loading:
            return
        td = self._type_dict(type_name, create=True)
        if td is None:
            return
        if text == "(none)" or not text.strip():
            td.pop("extends", None)
        else:
            td["extends"] = text.strip()
        self._on_model_changed()
        # A variation's base drives both its selector label and which catalog of
        # entries it can override, so refresh the selector and rebuild the editor.
        if not self._is_base_type(type_name):
            self._populate_type_combo()
            self._rebuild_type_editor()

    def _build_color_row(self, type_name: str, entry: str) -> QWidget:
        widget = QWidget()
        row = QHBoxLayout(widget)
        row.setContentsMargins(0, 2, 0, 2)
        row.setSpacing(4)

        label = QLabel(entry)
        label.setMinimumWidth(110)
        mode = QComboBox()
        mode.addItems(["Inherit", "Literal", "Palette"])
        color_btn = ColorButton(color="#888888")
        key_combo = QComboBox()
        for key in self._palette_keys():
            key_combo.addItem(key)
        op_combo = QComboBox()
        op_combo.addItems(COLOR_OPS)
        amount = QDoubleSpinBox()
        amount.setRange(0.0, 1.0)
        amount.setSingleStep(0.05)
        amount.setDecimals(3)
        amount.setValue(0.1)

        row.addWidget(label)
        row.addWidget(mode)
        row.addWidget(color_btn)
        row.addWidget(key_combo)
        row.addWidget(op_combo)
        row.addWidget(amount)
        row.addStretch()

        val = self._get_entry_value(type_name, "colors", entry)
        if isinstance(val, dict) and "value" in val:
            mode.setCurrentText("Literal")
            color_btn.set_color(self._rgba_to_hex(self._coerce_rgba(val.get("value"))))
        elif isinstance(val, dict) and isinstance(val.get("bind"), str) \
                and val["bind"].startswith("palette:"):
            mode.setCurrentText("Palette")
            key = val["bind"].split(":", 1)[1]
            if key_combo.findText(key) < 0:
                key_combo.addItem(key)
            key_combo.setCurrentText(key)
            op = str(val.get("op", "none") or "none")
            if op_combo.findText(op) < 0:
                op = "none"
            op_combo.setCurrentText(op)
            amount.setValue(float(val.get("amount", 0.0) or 0.0))
        else:
            mode.setCurrentText("Inherit")

        def update_visibility() -> None:
            current = mode.currentText()
            color_btn.setVisible(current == "Literal")
            is_palette = current == "Palette"
            key_combo.setVisible(is_palette)
            op_combo.setVisible(is_palette)
            amount.setVisible(is_palette)

        def apply() -> None:
            current = mode.currentText()
            if current == "Inherit":
                self._set_entry_value(type_name, "colors", entry, None)
            elif current == "Literal":
                self._set_entry_value(type_name, "colors", entry,
                                      {"value": self._hex_to_rgba(color_btn.get_color())})
            else:
                key = key_combo.currentText()
                out: Dict[str, Any] = {"bind": f"palette:{key}"}
                op = op_combo.currentText()
                if op and op != "none":
                    out["op"] = op
                    out["amount"] = float(amount.value())
                self._set_entry_value(type_name, "colors", entry, out)
            update_visibility()
            self._on_model_changed()

        update_visibility()
        mode.currentTextChanged.connect(lambda _text: apply())
        color_btn.color_changed.connect(lambda _hex: apply())
        key_combo.currentTextChanged.connect(lambda _text: apply())
        op_combo.currentTextChanged.connect(lambda _text: apply())
        amount.valueChanged.connect(lambda _value: apply())
        return widget

    def _build_constant_row(self, type_name: str, entry: str,
                            label: Optional[str] = None) -> QWidget:
        widget = QWidget()
        row = QHBoxLayout(widget)
        row.setContentsMargins(0, 2, 0, 2)
        row.setSpacing(4)

        label_widget = QLabel(label if label is not None else entry)
        label_widget.setMinimumWidth(110)
        mode = QComboBox()
        mode.addItems(["Inherit", "Literal", "Variable"])
        spin = QDoubleSpinBox()
        spin.setRange(-1000000.0, 1000000.0)
        spin.setDecimals(3)
        var_combo = QComboBox()
        for key in self._variable_keys():
            var_combo.addItem(key)

        row.addWidget(label_widget)
        row.addWidget(mode)
        row.addWidget(spin)
        row.addWidget(var_combo)
        row.addStretch()

        val = self._get_entry_value(type_name, "constants", entry)
        if isinstance(val, dict) and "value" in val:
            mode.setCurrentText("Literal")
            try:
                spin.setValue(float(val.get("value")))
            except (TypeError, ValueError):
                spin.setValue(0.0)
        elif isinstance(val, dict) and isinstance(val.get("bind"), str) \
                and val["bind"].startswith("var:"):
            mode.setCurrentText("Variable")
            key = val["bind"].split(":", 1)[1]
            if var_combo.findText(key) < 0:
                var_combo.addItem(key)
            var_combo.setCurrentText(key)
        else:
            mode.setCurrentText("Inherit")

        def update_visibility() -> None:
            current = mode.currentText()
            spin.setVisible(current == "Literal")
            var_combo.setVisible(current == "Variable")

        def apply() -> None:
            current = mode.currentText()
            if current == "Inherit":
                self._set_entry_value(type_name, "constants", entry, None)
            elif current == "Literal":
                self._set_entry_value(type_name, "constants", entry,
                                      {"value": float(spin.value())})
            else:
                key = var_combo.currentText()
                self._set_entry_value(type_name, "constants", entry,
                                      {"bind": f"var:{key}"})
            update_visibility()
            self._on_model_changed()

        update_visibility()
        mode.currentTextChanged.connect(lambda _text: apply())
        spin.valueChanged.connect(lambda _value: apply())
        var_combo.currentTextChanged.connect(lambda _text: apply())
        return widget

    def _coerce_vec2(self, value: Any) -> List[float]:
        """Parse a Vec2 value (an [x,y] array or {"x","y"} object) to [x, y] floats."""
        try:
            if isinstance(value, dict):
                return [float(value.get("x", 0.0)), float(value.get("y", 0.0))]
            seq = list(value)
            return [float(seq[0]), float(seq[1])]
        except (TypeError, ValueError, IndexError):
            return [0.0, 0.0]

    def _build_modulation_row(self, type_name: str, entry: str,
                              label: Optional[str] = None) -> QWidget:
        """A per-state modulation row: 4 multiplier channels (R,G,B,A) or Inherit.

        Modulation is a multiplier (values may exceed 1.0 to brighten), so it uses
        plain float channels rather than a 0-255 colour picker. Stored under the
        type's "colors" category as { "value": [r,g,b,a] }.
        """
        widget = QWidget()
        row = QHBoxLayout(widget)
        row.setContentsMargins(0, 2, 0, 2)
        row.setSpacing(4)

        label_widget = QLabel(label if label is not None else entry)
        label_widget.setMinimumWidth(110)
        mode = QComboBox()
        mode.addItems(["Inherit", "Literal"])
        spins: List[QDoubleSpinBox] = []
        for _ in range(4):
            spin = QDoubleSpinBox()
            spin.setRange(0.0, 8.0)
            spin.setSingleStep(0.05)
            spin.setDecimals(3)
            spin.setValue(1.0)
            spins.append(spin)

        row.addWidget(label_widget)
        row.addWidget(mode)
        for spin in spins:
            row.addWidget(spin)
        row.addStretch()

        val = self._get_entry_value(type_name, "colors", entry)
        if isinstance(val, dict) and "value" in val:
            mode.setCurrentText("Literal")
            rgba = self._coerce_rgba(val.get("value"))
            for i in range(4):
                spins[i].setValue(float(rgba[i]))
        else:
            mode.setCurrentText("Inherit")

        def update_visibility() -> None:
            literal = mode.currentText() == "Literal"
            for spin in spins:
                spin.setVisible(literal)

        def apply() -> None:
            if mode.currentText() == "Inherit":
                self._set_entry_value(type_name, "colors", entry, None)
            else:
                self._set_entry_value(type_name, "colors", entry,
                                      {"value": [float(spin.value()) for spin in spins]})
            update_visibility()
            self._on_model_changed()

        update_visibility()
        mode.currentTextChanged.connect(lambda _text: apply())
        for spin in spins:
            spin.valueChanged.connect(lambda _value: apply())
        return widget

    def _build_vec2_row(self, type_name: str, entry: str,
                        label: Optional[str] = None) -> QWidget:
        """A per-state Vec2 row (tween scale/position): X/Y literals or Inherit."""
        widget = QWidget()
        row = QHBoxLayout(widget)
        row.setContentsMargins(0, 2, 0, 2)
        row.setSpacing(4)

        label_widget = QLabel(label if label is not None else entry)
        label_widget.setMinimumWidth(110)
        mode = QComboBox()
        mode.addItems(["Inherit", "Literal"])
        x_spin = QDoubleSpinBox()
        x_spin.setRange(-100000.0, 100000.0)
        x_spin.setDecimals(3)
        x_spin.setSingleStep(0.05)
        y_spin = QDoubleSpinBox()
        y_spin.setRange(-100000.0, 100000.0)
        y_spin.setDecimals(3)
        y_spin.setSingleStep(0.05)

        row.addWidget(label_widget)
        row.addWidget(mode)
        row.addWidget(x_spin)
        row.addWidget(y_spin)
        row.addStretch()

        val = self._get_entry_value(type_name, "vec2s", entry)
        if isinstance(val, dict) and "value" in val:
            mode.setCurrentText("Literal")
            xy = self._coerce_vec2(val.get("value"))
            x_spin.setValue(xy[0])
            y_spin.setValue(xy[1])
        else:
            mode.setCurrentText("Inherit")

        def update_visibility() -> None:
            literal = mode.currentText() == "Literal"
            x_spin.setVisible(literal)
            y_spin.setVisible(literal)

        def apply() -> None:
            if mode.currentText() == "Inherit":
                self._set_entry_value(type_name, "vec2s", entry, None)
            else:
                self._set_entry_value(type_name, "vec2s", entry,
                                      {"value": [float(x_spin.value()), float(y_spin.value())]})
            update_visibility()
            self._on_model_changed()

        update_visibility()
        mode.currentTextChanged.connect(lambda _text: apply())
        x_spin.valueChanged.connect(lambda _value: apply())
        y_spin.valueChanged.connect(lambda _value: apply())
        return widget

    def _build_bool_row(self, type_name: str, entry: str,
                        label: Optional[str] = None) -> QWidget:
        """A per-state bool row (tween enabled): a literal checkbox or Inherit."""
        widget = QWidget()
        row = QHBoxLayout(widget)
        row.setContentsMargins(0, 2, 0, 2)
        row.setSpacing(4)

        label_widget = QLabel(label if label is not None else entry)
        label_widget.setMinimumWidth(110)
        mode = QComboBox()
        mode.addItems(["Inherit", "Literal"])
        check = QCheckBox()

        row.addWidget(label_widget)
        row.addWidget(mode)
        row.addWidget(check)
        row.addStretch()

        val = self._get_entry_value(type_name, "bools", entry)
        if isinstance(val, dict) and "value" in val:
            mode.setCurrentText("Literal")
            check.setChecked(bool(val.get("value")))
        else:
            mode.setCurrentText("Inherit")

        def update_visibility() -> None:
            check.setVisible(mode.currentText() == "Literal")

        def apply() -> None:
            if mode.currentText() == "Inherit":
                self._set_entry_value(type_name, "bools", entry, None)
            else:
                self._set_entry_value(type_name, "bools", entry,
                                      {"value": bool(check.isChecked())})
            update_visibility()
            self._on_model_changed()

        update_visibility()
        mode.currentTextChanged.connect(lambda _text: apply())
        check.toggled.connect(lambda _checked: apply())
        return widget

    def _build_font_row(self, type_name: str, entry: str) -> QWidget:
        widget = QWidget()
        row = QHBoxLayout(widget)
        row.setContentsMargins(0, 2, 0, 2)
        row.setSpacing(4)

        label = QLabel(entry)
        label.setMinimumWidth(110)
        mode = QComboBox()
        mode.addItems(["Inherit", "Role", "Custom"])
        role_combo = QComboBox()
        for name in self._font_role_names():
            role_combo.addItem(name)
        path_widget = PathPropertyWidget("Font")
        size_spin = QDoubleSpinBox()
        size_spin.setRange(1.0, 512.0)
        size_spin.setDecimals(1)
        size_spin.setValue(16.0)

        row.addWidget(label)
        row.addWidget(mode)
        row.addWidget(role_combo)
        row.addWidget(path_widget, 1)
        row.addWidget(size_spin)
        row.addStretch()

        val = self._get_entry_value(type_name, "fonts", entry)
        if isinstance(val, dict) and "role" in val:
            mode.setCurrentText("Role")
            role = str(val.get("role", ""))
            if role_combo.findText(role) < 0:
                role_combo.addItem(role)
            role_combo.setCurrentText(role)
        elif isinstance(val, dict) and "font" in val:
            mode.setCurrentText("Custom")
            path_widget.set_value(str(val.get("font", "")))
            try:
                size_spin.setValue(float(val.get("size", 16.0)))
            except (TypeError, ValueError):
                size_spin.setValue(16.0)
        else:
            mode.setCurrentText("Inherit")

        def update_visibility() -> None:
            current = mode.currentText()
            role_combo.setVisible(current == "Role")
            path_widget.setVisible(current == "Custom")
            size_spin.setVisible(current == "Custom")

        def apply() -> None:
            current = mode.currentText()
            if current == "Inherit":
                self._set_entry_value(type_name, "fonts", entry, None)
            elif current == "Role":
                self._set_entry_value(type_name, "fonts", entry,
                                      {"role": role_combo.currentText()})
            else:
                self._set_entry_value(type_name, "fonts", entry,
                                      {"font": path_widget.get_value(),
                                       "size": float(size_spin.value())})
            update_visibility()
            self._on_model_changed()

        update_visibility()
        mode.currentTextChanged.connect(lambda _text: apply())
        role_combo.currentTextChanged.connect(lambda _text: apply())
        path_widget.value_changed.connect(lambda _value: apply())
        size_spin.valueChanged.connect(lambda _value: apply())
        return widget

    def _build_image_row(self, type_name: str, entry: str) -> QWidget:
        widget = QWidget()
        outer = QVBoxLayout(widget)
        outer.setContentsMargins(0, 2, 0, 2)
        outer.setSpacing(3)

        top = QHBoxLayout()
        top.setSpacing(4)
        label = QLabel(entry)
        label.setMinimumWidth(110)
        mode = QComboBox()
        mode.addItems(["Inherit", "Custom"])
        path_widget = PathPropertyWidget("Image")
        top.addWidget(label)
        top.addWidget(mode)
        top.addWidget(path_widget, 1)
        outer.addLayout(top)

        # Stretch / nine-slice sub-panel: the theme can dictate HOW the image is fitted
        # (e.g. set a texture-button image to nine-slice and supply the margins).
        fit_panel = QWidget()
        fit_layout = QVBoxLayout(fit_panel)
        fit_layout.setContentsMargins(114, 0, 0, 0)
        fit_layout.setSpacing(2)

        fit_row = QHBoxLayout()
        fit_row.setSpacing(4)
        fit_row.addWidget(QLabel("Fit"))
        stretch_combo = QComboBox()
        stretch_combo.addItems(["Inherit", "Stretch", "Keep Centered", "Nine-Slice"])
        fit_row.addWidget(stretch_combo, 1)
        fit_layout.addLayout(fit_row)

        ns_panel = QWidget()
        ns_layout = QVBoxLayout(ns_panel)
        ns_layout.setContentsMargins(0, 0, 0, 0)
        ns_layout.setSpacing(2)

        margin_keys = ["left", "top", "right", "bottom"]
        margin_spins: Dict[str, QDoubleSpinBox] = {}
        margin_row = QHBoxLayout()
        margin_row.setSpacing(4)
        margin_row.addWidget(QLabel("Margins"))
        for key in margin_keys:
            spin = QDoubleSpinBox()
            spin.setRange(0.0, 4096.0)
            spin.setDecimals(1)
            spin.setSingleStep(1.0)
            spin.setToolTip(key.capitalize() + " margin (source texture pixels)")
            margin_spins[key] = spin
            margin_row.addWidget(spin)
        ns_layout.addLayout(margin_row)

        axis_row = QHBoxLayout()
        axis_row.setSpacing(4)
        axis_row.addWidget(QLabel("Axis H/V"))
        axis_h = QComboBox()
        axis_h.addItems(["Stretch", "Tile"])
        axis_v = QComboBox()
        axis_v.addItems(["Stretch", "Tile"])
        draw_center = QCheckBox("Draw center")
        draw_center.setChecked(True)
        axis_row.addWidget(axis_h)
        axis_row.addWidget(axis_v)
        axis_row.addWidget(draw_center)
        axis_row.addStretch()
        ns_layout.addLayout(axis_row)

        fit_layout.addWidget(ns_panel)
        outer.addWidget(fit_panel)

        # ThemeImage serializes as {"path": "res://..", "stretch_mode": "..",
        # "nine_slice": {..}}; a bare string (path only) is also accepted.
        val = self._get_entry_value(type_name, "images", entry)
        path_str = ""
        stretch_mode = ""
        nine = {}
        if isinstance(val, dict) and val.get("path"):
            path_str = str(val.get("path", ""))
            stretch_mode = str(val.get("stretch_mode", ""))
            nine = val.get("nine_slice", {}) if isinstance(val.get("nine_slice"), dict) else {}
        elif isinstance(val, str) and val:
            path_str = val

        if path_str:
            mode.setCurrentText("Custom")
            path_widget.set_value(path_str)
        else:
            mode.setCurrentText("Inherit")

        stretch_index = {"": 0, "stretch": 1, "keep_centered": 2, "nine_slice": 3}.get(stretch_mode, 0)
        stretch_combo.setCurrentIndex(stretch_index)
        for key in margin_keys:
            margin_spins[key].setValue(float(nine.get(key, 8.0)))
        axis_h.setCurrentIndex(1 if nine.get("axis_h") == "tile" else 0)
        axis_v.setCurrentIndex(1 if nine.get("axis_v") == "tile" else 0)
        draw_center.setChecked(bool(nine.get("draw_center", True)))

        def update_visibility() -> None:
            is_custom = mode.currentText() == "Custom"
            path_widget.setVisible(is_custom)
            fit_panel.setVisible(is_custom)
            ns_panel.setVisible(is_custom and stretch_combo.currentIndex() == 3)

        def apply() -> None:
            if mode.currentText() == "Inherit":
                self._set_entry_value(type_name, "images", entry, None)
                update_visibility()
                self._on_model_changed()
                return
            data: Dict[str, Any] = {"path": path_widget.get_value()}
            index = stretch_combo.currentIndex()
            if index == 1:
                data["stretch_mode"] = "stretch"
            elif index == 2:
                data["stretch_mode"] = "keep_centered"
            elif index == 3:
                data["stretch_mode"] = "nine_slice"
                data["nine_slice"] = {
                    "left": margin_spins["left"].value(),
                    "top": margin_spins["top"].value(),
                    "right": margin_spins["right"].value(),
                    "bottom": margin_spins["bottom"].value(),
                    "axis_h": "tile" if axis_h.currentIndex() == 1 else "stretch",
                    "axis_v": "tile" if axis_v.currentIndex() == 1 else "stretch",
                    "draw_center": draw_center.isChecked(),
                }
            self._set_entry_value(type_name, "images", entry, data)
            update_visibility()
            self._on_model_changed()

        update_visibility()
        mode.currentTextChanged.connect(lambda _text: apply())
        path_widget.value_changed.connect(lambda _value: apply())
        stretch_combo.currentIndexChanged.connect(lambda _index: apply())
        for spin in margin_spins.values():
            spin.valueChanged.connect(lambda _value: apply())
        axis_h.currentIndexChanged.connect(lambda _index: apply())
        axis_v.currentIndexChanged.connect(lambda _index: apply())
        draw_center.toggled.connect(lambda _checked: apply())
        return widget

    # ------------------------------------------------------------- styleboxes

    def _default_stylebox(self, subtype: str) -> Dict[str, Any]:
        """A fresh concrete stylebox document of `subtype` with sensible defaults."""
        if subtype == "StyleBoxTexture":
            return {"type": "StyleBoxTexture", "texturePath": "", "drawCenter": True,
                    "modulateColor": {"r": 1.0, "g": 1.0, "b": 1.0, "a": 1.0},
                    "axisStretchHorizontal": "stretch", "axisStretchVertical": "stretch"}
        if subtype == "StyleBoxLine":
            return {"type": "StyleBoxLine",
                    "color": {"r": 1.0, "g": 1.0, "b": 1.0, "a": 1.0},
                    "thickness": 1.0, "vertical": False}
        if subtype == "StyleBoxEmpty":
            return {"type": "StyleBoxEmpty"}
        return {"type": "StyleBoxFlat",
                "backgroundColor": {"r": 0.2, "g": 0.2, "b": 0.2, "a": 1.0}}

    def _box_color(self, box: Dict[str, Any], key: str,
                   default=(1.0, 1.0, 1.0, 1.0)) -> List[float]:
        """Read a {r,g,b,a} stylebox colour field into an rgba float list."""
        col = box.get(key)
        if isinstance(col, dict):
            return [float(col.get("r", default[0])), float(col.get("g", default[1])),
                    float(col.get("b", default[2])), float(col.get("a", default[3]))]
        return [float(default[0]), float(default[1]),
                float(default[2]), float(default[3])]

    def _set_box_color(self, box: Dict[str, Any], key: str, rgba: List[float]) -> None:
        """Write an rgba list back to a {r,g,b,a} stylebox colour field."""
        values = self._coerce_rgba(rgba)
        box[key] = {"r": values[0], "g": values[1], "b": values[2], "a": values[3]}

    def _sb_row(self, label: str, *controls) -> QWidget:
        """A labelled stylebox field row holding the given control widgets."""
        widget = QWidget()
        row = QHBoxLayout(widget)
        row.setContentsMargins(0, 2, 0, 2)
        row.setSpacing(4)
        label_widget = QLabel(label)
        label_widget.setMinimumWidth(150)
        row.addWidget(label_widget)
        for control in controls:
            row.addWidget(control)
        row.addStretch()
        return widget

    def _sb_number_row(self, box: Dict[str, Any], key: str, label: str,
                       default: float = 0.0) -> QWidget:
        spin = QDoubleSpinBox()
        spin.setRange(-1000000.0, 1000000.0)
        spin.setDecimals(3)
        try:
            spin.setValue(float(box.get(key, default)))
        except (TypeError, ValueError):
            spin.setValue(float(default))

        def on_changed(value: float) -> None:
            box[key] = float(value)
            self._on_model_changed()

        spin.valueChanged.connect(on_changed)
        return self._sb_row(label, spin)

    def _sb_bool_row(self, box: Dict[str, Any], key: str, label: str,
                     default: bool = False) -> QWidget:
        check = QCheckBox()
        check.setChecked(bool(box.get(key, default)))

        def on_toggled(value: bool) -> None:
            box[key] = bool(value)
            self._on_model_changed()

        check.toggled.connect(on_toggled)
        return self._sb_row(label, check)

    def _sb_string_row(self, box: Dict[str, Any], key: str, label: str) -> QWidget:
        edit = QLineEdit(str(box.get(key, "") or ""))

        def on_finished() -> None:
            box[key] = edit.text()
            self._on_model_changed()

        edit.editingFinished.connect(on_finished)
        return self._sb_row(label, edit)

    def _sb_path_row(self, box: Dict[str, Any], key: str, label: str) -> QWidget:
        path_widget = PathPropertyWidget("Texture")
        path_widget.set_value(str(box.get(key, "") or ""))

        def on_changed(value: str) -> None:
            box[key] = value
            self._on_model_changed()

        path_widget.value_changed.connect(on_changed)
        return self._sb_row(label, path_widget)

    def _sb_enum_row(self, box: Dict[str, Any], key: str, label: str,
                     options: List[str], default: str) -> QWidget:
        combo = QComboBox()
        combo.addItems(options)
        current = str(box.get(key, default) or default)
        if combo.findText(current) < 0:
            current = default
        combo.setCurrentText(current)

        def on_changed(text: str) -> None:
            box[key] = text
            self._on_model_changed()

        combo.currentTextChanged.connect(on_changed)
        return self._sb_row(label, combo)

    def _sb_vec2_row(self, box: Dict[str, Any], key: str, label: str) -> QWidget:
        cur = box.get(key)
        cur = cur if isinstance(cur, dict) else {}
        x_spin = QDoubleSpinBox()
        x_spin.setRange(-1000000.0, 1000000.0)
        x_spin.setDecimals(3)
        x_spin.setValue(float(cur.get("x", 0.0)))
        y_spin = QDoubleSpinBox()
        y_spin.setRange(-1000000.0, 1000000.0)
        y_spin.setDecimals(3)
        y_spin.setValue(float(cur.get("y", 0.0)))

        def on_changed(_value=None) -> None:
            box[key] = {"x": float(x_spin.value()), "y": float(y_spin.value())}
            self._on_model_changed()

        x_spin.valueChanged.connect(on_changed)
        y_spin.valueChanged.connect(on_changed)
        return self._sb_row(label, x_spin, y_spin)

    def _sb_rect_row(self, box: Dict[str, Any], key: str, label: str) -> QWidget:
        cur = box.get(key)
        cur = cur if isinstance(cur, dict) else {}
        spins: Dict[str, QDoubleSpinBox] = {}
        controls: List[QWidget] = []
        for axis in ("x", "y", "w", "h"):
            spin = QDoubleSpinBox()
            spin.setRange(-1000000.0, 1000000.0)
            spin.setDecimals(3)
            spin.setValue(float(cur.get(axis, 0.0)))
            spins[axis] = spin
            controls.append(QLabel(axis))
            controls.append(spin)

        def on_changed(_value=None) -> None:
            box[key] = {axis: float(spin.value()) for axis, spin in spins.items()}
            self._on_model_changed()

        for spin in spins.values():
            spin.valueChanged.connect(on_changed)
        return self._sb_row(label, *controls)

    def _sb_color_row(self, box: Dict[str, Any], key: str, label: str,
                      default=(1.0, 1.0, 1.0, 1.0)) -> QWidget:
        """A literal {r,g,b,a} stylebox colour row (swatch + alpha)."""
        rgba = self._box_color(box, key, default)
        color_btn = ColorButton(color=self._rgba_to_hex(rgba))
        alpha = QDoubleSpinBox()
        alpha.setRange(0.0, 1.0)
        alpha.setSingleStep(0.05)
        alpha.setDecimals(3)
        alpha.setValue(rgba[3])

        def apply(_value=None) -> None:
            col = self._hex_to_rgba(color_btn.get_color(), float(alpha.value()))
            self._set_box_color(box, key, col)
            self._on_model_changed()

        color_btn.color_changed.connect(lambda _hex: apply())
        alpha.valueChanged.connect(lambda _value: apply())
        return self._sb_row(label, color_btn, alpha)

    def _sb_color_binding_row(self, box: Dict[str, Any], bindings: Dict[str, Any],
                              key: str, label: str,
                              default=(1.0, 1.0, 1.0, 1.0)) -> QWidget:
        """A StyleBoxFlat colour row that can be a literal or a palette binding.

        The literal {r,g,b,a} value is always written to the box (the resolve-time
        fallback); a palette binding, when chosen, is written to the entry's
        `bindings` map in the same shape as a themed colour entry.
        """
        widget = QWidget()
        row = QHBoxLayout(widget)
        row.setContentsMargins(0, 2, 0, 2)
        row.setSpacing(4)

        label_widget = QLabel(label)
        label_widget.setMinimumWidth(150)
        mode = QComboBox()
        mode.addItems(["Literal", "Palette"])
        rgba = self._box_color(box, key, default)
        color_btn = ColorButton(color=self._rgba_to_hex(rgba))
        alpha = QDoubleSpinBox()
        alpha.setRange(0.0, 1.0)
        alpha.setSingleStep(0.05)
        alpha.setDecimals(3)
        alpha.setValue(rgba[3])
        key_combo = QComboBox()
        for palette_key in self._palette_keys():
            key_combo.addItem(palette_key)
        op_combo = QComboBox()
        op_combo.addItems(COLOR_OPS)
        amount = QDoubleSpinBox()
        amount.setRange(0.0, 1.0)
        amount.setSingleStep(0.05)
        amount.setDecimals(3)

        row.addWidget(label_widget)
        row.addWidget(mode)
        row.addWidget(color_btn)
        row.addWidget(alpha)
        row.addWidget(key_combo)
        row.addWidget(op_combo)
        row.addWidget(amount)
        row.addStretch()

        bind = bindings.get(key)
        if isinstance(bind, dict) and isinstance(bind.get("bind"), str) \
                and bind["bind"].startswith("palette:"):
            mode.setCurrentText("Palette")
            palette_key = bind["bind"].split(":", 1)[1]
            if key_combo.findText(palette_key) < 0:
                key_combo.addItem(palette_key)
            key_combo.setCurrentText(palette_key)
            op = str(bind.get("op", "none") or "none")
            if op_combo.findText(op) < 0:
                op = "none"
            op_combo.setCurrentText(op)
            amount.setValue(float(bind.get("amount", 0.0) or 0.0))
        else:
            mode.setCurrentText("Literal")

        def update_visibility() -> None:
            literal = mode.currentText() == "Literal"
            color_btn.setVisible(literal)
            alpha.setVisible(literal)
            key_combo.setVisible(not literal)
            op_combo.setVisible(not literal)
            amount.setVisible(not literal)

        def apply(_value=None) -> None:
            col = self._hex_to_rgba(color_btn.get_color(), float(alpha.value()))
            self._set_box_color(box, key, col)
            if mode.currentText() == "Palette":
                out: Dict[str, Any] = {"bind": f"palette:{key_combo.currentText()}"}
                op = op_combo.currentText()
                if op and op != "none":
                    out["op"] = op
                    out["amount"] = float(amount.value())
                bindings[key] = out
            else:
                bindings.pop(key, None)
            update_visibility()
            self._on_model_changed()

        update_visibility()
        mode.currentTextChanged.connect(lambda _text: apply())
        color_btn.color_changed.connect(lambda _hex: apply())
        alpha.valueChanged.connect(lambda _value: apply())
        key_combo.currentTextChanged.connect(lambda _text: apply())
        op_combo.currentTextChanged.connect(lambda _text: apply())
        amount.valueChanged.connect(lambda _value: apply())
        return widget

    def _build_stylebox_fields(self, subtype: str, box: Dict[str, Any],
                               bindings: Dict[str, Any]) -> List[QWidget]:
        """Build the editable field rows for one concrete stylebox `subtype`."""
        sides = ("Left", "Right", "Top", "Bottom")
        rows: List[QWidget] = []
        if subtype == "StyleBoxFlat":
            rows.append(self._sb_color_binding_row(
                box, bindings, "backgroundColor", "Background Color", (0.2, 0.2, 0.2, 1.0)))
            rows.append(self._sb_number_row(box, "opacity", "Opacity", 1.0))
            rows.append(self._sb_bool_row(box, "borderWidthLinked", "Border Width Linked", True))
            for side in sides:
                rows.append(self._sb_number_row(
                    box, f"borderWidth{side}", f"Border Width {side}", 0.0))
            rows.append(self._sb_bool_row(box, "borderColorLinked", "Border Color Linked", True))
            for side in sides:
                rows.append(self._sb_color_binding_row(
                    box, bindings, f"borderColor{side}", f"Border Color {side}", (0.0, 0.0, 0.0, 1.0)))
            rows.append(self._sb_bool_row(box, "cornerRadiusLinked", "Corner Radius Linked", True))
            for corner in ("TopLeft", "TopRight", "BottomLeft", "BottomRight"):
                rows.append(self._sb_number_row(
                    box, f"cornerRadius{corner}", f"Corner Radius {corner}", 0.0))
            rows.append(self._sb_number_row(box, "cornerDetail", "Corner Detail", 8.0))
            rows.append(self._sb_bool_row(box, "antiAliasing", "Anti Aliasing", True))
            rows.append(self._sb_number_row(box, "antiAliasingSize", "Anti Aliasing Size", 1.0))
            for side in sides:
                rows.append(self._sb_number_row(
                    box, f"expandMargin{side}", f"Expand Margin {side}", 0.0))
            rows.append(self._sb_bool_row(box, "shadowEnabled", "Shadow Enabled", False))
            rows.append(self._sb_color_row(box, "shadowColor", "Shadow Color", (0.0, 0.0, 0.0, 1.0)))
            rows.append(self._sb_number_row(box, "shadowSize", "Shadow Size", 0.0))
            rows.append(self._sb_vec2_row(box, "shadowOffset", "Shadow Offset"))
            for side in sides:
                rows.append(self._sb_number_row(
                    box, f"contentMargin{side}", f"Content Margin {side}", 0.0))
        elif subtype == "StyleBoxTexture":
            rows.append(self._sb_path_row(box, "texturePath", "Texture Path"))
            rows.append(self._sb_rect_row(box, "regionRect", "Region Rect"))
            rows.append(self._sb_color_row(box, "modulateColor", "Modulate Color", (1.0, 1.0, 1.0, 1.0)))
            rows.append(self._sb_bool_row(box, "drawCenter", "Draw Center", True))
            for side in sides:
                rows.append(self._sb_number_row(
                    box, f"textureMargin{side}", f"Texture Margin {side}", 0.0))
            for side in sides:
                rows.append(self._sb_number_row(
                    box, f"expandMargin{side}", f"Expand Margin {side}", 0.0))
            rows.append(self._sb_enum_row(
                box, "axisStretchHorizontal", "Axis Stretch Horizontal",
                AXIS_STRETCH_MODES, "stretch"))
            rows.append(self._sb_enum_row(
                box, "axisStretchVertical", "Axis Stretch Vertical",
                AXIS_STRETCH_MODES, "stretch"))
            for side in sides:
                rows.append(self._sb_number_row(
                    box, f"contentMargin{side}", f"Content Margin {side}", 0.0))
        elif subtype == "StyleBoxLine":
            rows.append(self._sb_color_row(box, "color", "Color", (1.0, 1.0, 1.0, 1.0)))
            rows.append(self._sb_number_row(box, "growBegin", "Grow Begin", 0.0))
            rows.append(self._sb_number_row(box, "growEnd", "Grow End", 0.0))
            rows.append(self._sb_number_row(box, "thickness", "Thickness", 1.0))
            rows.append(self._sb_bool_row(box, "vertical", "Vertical", False))
            for side in sides:
                rows.append(self._sb_number_row(
                    box, f"contentMargin{side}", f"Content Margin {side}", 0.0))
        else:
            for side in sides:
                rows.append(self._sb_number_row(
                    box, f"contentMargin{side}", f"Content Margin {side}", 0.0))
        return rows

    def _build_stylebox_entry(self, type_name: str, entry_name: str) -> QWidget:
        """Build the collapsible editor for one named stylebox entry of a type."""
        cat = self._category_dict(type_name, "styleboxes", create=True)
        entry = cat.get(entry_name)
        if not isinstance(entry, dict):
            entry = {}
            cat[entry_name] = entry
        box = entry.get("stylebox")
        if not isinstance(box, dict):
            box = {"type": "StyleBoxFlat"}
            entry["stylebox"] = box
        bindings = entry.get("bindings")
        if not isinstance(bindings, dict):
            bindings = {}
            entry["bindings"] = bindings
        subtype = str(box.get("type", "StyleBoxFlat") or "StyleBoxFlat")
        if subtype not in STYLEBOX_TYPES:
            subtype = "StyleBoxFlat"

        group = CollapsibleGroupBox(entry_name)

        header = QWidget()
        header_row = QHBoxLayout(header)
        header_row.setContentsMargins(0, 0, 0, 0)
        header_row.setSpacing(4)
        header_row.addWidget(QLabel("Subtype:"))
        type_combo = QComboBox()
        type_combo.addItems(STYLEBOX_TYPES)
        type_combo.setCurrentText(subtype)
        type_combo.currentTextChanged.connect(
            lambda text, tn=type_name, en=entry_name:
            self._on_stylebox_type_changed(tn, en, text))
        header_row.addWidget(type_combo, 1)
        remove_btn = QPushButton("−")
        remove_btn.setProperty("danger", True)
        remove_btn.setFixedWidth(28)
        remove_btn.clicked.connect(
            lambda _checked, tn=type_name, en=entry_name:
            self._on_remove_stylebox(tn, en))
        header_row.addWidget(remove_btn)
        group.add_widget(header)

        for field_widget in self._build_stylebox_fields(subtype, box, bindings):
            group.add_widget(field_widget)
        return group

    def _build_styleboxes_section(self, type_name: str) -> None:
        """Append the per-type stylebox authoring section."""
        group = CollapsibleGroupBox("Styleboxes")
        cat = self._category_dict(type_name, "styleboxes")
        if isinstance(cat, dict):
            for entry_name in list(cat.keys()):
                if isinstance(cat.get(entry_name), dict):
                    group.add_widget(self._build_stylebox_entry(type_name, entry_name))

        add_row = QWidget()
        add_layout = QHBoxLayout(add_row)
        add_layout.setContentsMargins(0, 0, 0, 0)
        add_layout.setSpacing(4)
        name_edit = QLineEdit()
        name_edit.setPlaceholderText("stylebox name (e.g. panel)")
        add_layout.addWidget(name_edit, 1)
        add_btn = QPushButton("+ Add stylebox")
        add_btn.setProperty("success", True)
        add_btn.clicked.connect(
            lambda _checked, tn=type_name, e=name_edit:
            self._on_add_stylebox(tn, e.text()))
        add_layout.addWidget(add_btn)
        group.add_widget(add_row)

        self.type_editor_layout.addWidget(group)

    def _on_add_stylebox(self, type_name: str, entry_name: str) -> None:
        entry_name = (entry_name or "").strip()
        if not entry_name:
            QMessageBox.information(self, "Add Stylebox", "A stylebox needs a name.")
            return
        cat = self._category_dict(type_name, "styleboxes", create=True)
        if cat is None:
            return
        if entry_name in cat:
            QMessageBox.warning(self, "Add Stylebox",
                                f"A stylebox named '{entry_name}' already exists.")
            return
        cat[entry_name] = {"stylebox": self._default_stylebox("StyleBoxFlat"),
                           "bindings": {}}
        self._rebuild_type_editor()
        self._on_model_changed()

    def _on_remove_stylebox(self, type_name: str, entry_name: str) -> None:
        cat = self._category_dict(type_name, "styleboxes")
        if isinstance(cat, dict) and entry_name in cat:
            del cat[entry_name]
            if not cat:
                td = self._type_dict(type_name)
                if td is not None and "styleboxes" in td and not td["styleboxes"]:
                    del td["styleboxes"]
        self._rebuild_type_editor()
        self._on_model_changed()

    def _on_stylebox_type_changed(self, type_name: str, entry_name: str,
                                  subtype: str) -> None:
        cat = self._category_dict(type_name, "styleboxes")
        if not isinstance(cat, dict) or entry_name not in cat:
            return
        entry = cat.get(entry_name)
        if not isinstance(entry, dict):
            entry = {}
            cat[entry_name] = entry
        old_box = entry.get("stylebox")
        old_box = old_box if isinstance(old_box, dict) else {}
        new_box = self._default_stylebox(subtype)
        # Content margins are shared by every subtype; carry them across a switch.
        for side in ("Left", "Right", "Top", "Bottom"):
            margin_key = f"contentMargin{side}"
            if margin_key in old_box:
                new_box[margin_key] = old_box[margin_key]
        entry["stylebox"] = new_box
        # Palette bindings reference the previous subtype's colour fields; drop them.
        entry["bindings"] = {}
        self._rebuild_type_editor()
        self._on_model_changed()

    # --------------------------------------------------------------- preview

    def _on_model_changed(self) -> None:
        if self._loading:
            return
        self._refresh_preview()

    def _refresh_preview(self) -> None:
        if not hasattr(self, "pv_button"):
            return
        dark = self.pv_bg_dark.isChecked()
        frame_bg = "#1e1e22" if dark else "#f0f0f0"
        self.preview_frame.setStyleSheet(
            f"#uithemePreviewFrame {{ background-color: {frame_bg}; border: 1px solid #555; }}")

        btn_bg = self.resolve_color("Button", "background", [0.29, 0.56, 0.85, 1.0])
        btn_border = self.resolve_color("Button", "border_color", [0.0, 0.0, 0.0, 1.0])
        btn_fg = self.resolve_color("Button", "font_color", [1.0, 1.0, 1.0, 1.0])
        btn_radius = int(round(self.resolve_constant("Button", "corner_radius", 6.0)))
        btn_size = int(round(self.resolve_constant("Button", "font_size", 14.0)))
        self.pv_button.setStyleSheet(
            f"QPushButton {{ background-color: {self._rgba_to_hex(btn_bg)};"
            f" color: {self._rgba_to_hex(btn_fg)};"
            f" border: 1px solid {self._rgba_to_hex(btn_border)};"
            f" border-radius: {btn_radius}px; font-size: {btn_size}px; padding: 6px 16px; }}")

        dis_bg = self.apply_color_op(self.apply_color_op(btn_bg, "desaturate", 0.5), "darken", 0.15)
        dis_fg = self.apply_color_op(btn_fg, "darken", 0.3)
        self.pv_button_disabled.setStyleSheet(
            f"QPushButton {{ background-color: {self._rgba_to_hex(dis_bg)};"
            f" color: {self._rgba_to_hex(dis_fg)};"
            f" border: 1px solid {self._rgba_to_hex(btn_border)};"
            f" border-radius: {btn_radius}px; font-size: {btn_size}px; padding: 6px 16px; }}")

        lbl_fg = self.resolve_color("Label", "font_color", [0.9, 0.9, 0.9, 1.0])
        lbl_size = int(round(self.resolve_constant("Label", "font_size", 14.0)))
        self.pv_label.setStyleSheet(
            f"color: {self._rgba_to_hex(lbl_fg)}; font-size: {lbl_size}px;")

        le_bg = self.resolve_color("LineEdit", "background", [0.1, 0.1, 0.12, 1.0])
        le_border = self.resolve_color("LineEdit", "border_color", [0.4, 0.4, 0.4, 1.0])
        le_fg = self.resolve_color("LineEdit", "font_color", [0.9, 0.9, 0.9, 1.0])
        le_radius = int(round(self.resolve_constant("LineEdit", "corner_radius", 4.0)))
        le_size = int(round(self.resolve_constant("LineEdit", "font_size", 14.0)))
        self.pv_lineedit.setStyleSheet(
            f"QLineEdit {{ background-color: {self._rgba_to_hex(le_bg)};"
            f" color: {self._rgba_to_hex(le_fg)};"
            f" border: 1px solid {self._rgba_to_hex(le_border)};"
            f" border-radius: {le_radius}px; font-size: {le_size}px; padding: 4px; }}")

        cb_fg = self.resolve_color("Checkbox", "font_color", [0.9, 0.9, 0.9, 1.0])
        self.pv_checkbox.setStyleSheet(f"QCheckBox {{ color: {self._rgba_to_hex(cb_fg)}; }}")

        pn_bg = self.resolve_color("Panel", "background", [0.12, 0.12, 0.14, 1.0])
        pn_border = self.resolve_color("Panel", "border_color", [0.0, 0.0, 0.0, 1.0])
        pn_radius = int(round(self.resolve_constant("Panel", "corner_radius", 4.0)))
        self.pv_panel.setStyleSheet(
            f"#uithemePreviewPanel {{ background-color: {self._rgba_to_hex(pn_bg)};"
            f" border: 1px solid {self._rgba_to_hex(pn_border)};"
            f" border-radius: {pn_radius}px; }}")

    # ------------------------------------------------------- engine preview

    def _preview_theme_res(self) -> str:
        """res:// path of the theme being edited, or "" for a new unsaved theme."""
        if self.theme_path:
            return self._abs_to_res(self.theme_path)
        return ""

    def _add_preview_control(self, root, type_name: str, name: str,
                             x: float, y: float, w: float, h: float,
                             extra_props: Optional[Dict[str, Any]] = None,
                             rotation: float = 0.0,
                             scale: Optional[Dict[str, Any]] = None):
        """Create one themed preview control under `root` and return its record.

        Only non-color, layout/text properties are set so each control pulls its
        colors and fonts from the theme. When a theme file is known, the control's
        `theme` property is pointed at it so the preview reflects the edited theme.
        The returned record (or None on failure) is also appended to
        `self._preview_controls` for layout read-back and removal.
        """
        node = self.editor_bridge.create_node("Node2D", name)
        if node is None:
            print(f"UI Theme preview: create_node failed for {name}")
            return None
        root.add_child(node)
        comp = self.editor_bridge.create_component(type_name)
        if comp is None:
            print(f"UI Theme preview: create_component failed for {type_name}")
            return None
        node.add_component(comp)

        # Each property set is isolated so an unknown/typed property on one control
        # cannot abort the whole preview (and the failing one is reported).
        def _set_node(prop, value):
            try:
                self.editor_bridge.set_node_property_direct(node, prop, json.dumps(value))
            except Exception as exc:
                print(f"UI Theme preview: node '{prop}' on {type_name} failed: {exc}")

        def _set_comp(prop, value):
            try:
                self.editor_bridge.set_component_property_direct(comp, prop, json.dumps(value))
            except Exception as exc:
                print(f"UI Theme preview: component '{prop}' on {type_name} failed: {exc}")

        _set_node("position", {"x": float(x), "y": float(y)})
        if abs(float(rotation)) > 1e-9:
            _set_node("rotation", float(rotation))
        if scale is not None:
            _set_node("scale", {"x": float(scale.get("x", 1.0)),
                                "y": float(scale.get("y", 1.0))})
        _set_comp("width", float(w))
        _set_comp("height", float(h))

        theme_res = self._preview_theme_res()
        if theme_res:
            _set_comp("theme", theme_res)

        extra = dict(extra_props or {})
        for prop_name, prop_value in extra.items():
            _set_comp(prop_name, prop_value)

        record: Dict[str, Any] = {
            "node": node, "comp": comp, "type": type_name, "name": name,
            "extra": extra, "width": float(w), "height": float(h),
        }
        self._preview_controls.append(record)
        return record

    def _build_preview_scene(self) -> None:
        """Build a detached scene of real UI controls for the engine preview.

        Restores the project's saved preview layout when present; otherwise lays
        out a default sample gallery so a first-time user sees representative
        controls immediately.
        """
        import lupine_engine as le

        self._preview_scene = le.Scene("UIThemePreview")
        self._preview_controls = []
        root = self._preview_scene.get_root()

        saved = self._load_preview_layout()
        if saved:
            for entry in saved:
                if not isinstance(entry, dict):
                    continue
                type_name = str(entry.get("type", "")).strip()
                if not type_name:
                    continue
                name = str(entry.get("name") or "").strip() \
                    or self._unique_preview_name(type_name)
                extra = entry.get("extra")
                extra = extra if isinstance(extra, dict) else None
                scale = entry.get("scale")
                scale = scale if isinstance(scale, dict) else None
                self._add_preview_control(
                    root, type_name, name,
                    float(entry.get("x", 60.0)), float(entry.get("y", 60.0)),
                    float(entry.get("width", 160.0)), float(entry.get("height", 44.0)),
                    extra_props=extra,
                    rotation=float(entry.get("rotation", 0.0)),
                    scale=scale)
            if self._preview_controls:
                return
            # Nothing usable was restored; fall through to the default gallery.

        self._build_default_preview(root)

    def _build_default_preview(self, root) -> None:
        """Lay out the default sample gallery of UI controls under `root`.

        Colors/fonts are intentionally left unset so each control resolves them
        from the (edited) theme.
        """
        self._add_preview_control(root, "Panel", "PreviewPanel",
                                  40.0, 60.0, 320.0, 64.0)
        self._add_preview_control(root, "Label", "PreviewLabel",
                                  56.0, 78.0, 288.0, 28.0,
                                  extra_props={"text": "The quick brown fox - Label"})
        self._add_preview_control(root, "Button", "PreviewButton",
                                  40.0, 150.0, 150.0, 40.0,
                                  extra_props={"text": "Button"})
        self._add_preview_control(root, "Button", "PreviewButton2",
                                  210.0, 150.0, 150.0, 40.0,
                                  extra_props={"text": "Another"})
        self._add_preview_control(root, "LineEdit", "PreviewLineEdit",
                                  40.0, 220.0, 320.0, 36.0,
                                  extra_props={"placeholder": "Editable text"})
        self._add_preview_control(root, "Checkbox", "PreviewCheckbox",
                                  40.0, 280.0, 200.0, 30.0,
                                  extra_props={"text": "Checkbox"})
        self._add_preview_control(root, "ProgressBar", "PreviewProgress",
                                  40.0, 340.0, 320.0, 28.0)

    def _start_preview(self) -> None:
        """Initialize rendering for the embedded preview (deferred to post-show)."""
        vp = self._preview_viewport
        if vp is None or self._preview_scene is None:
            return
        try:
            import lupine_engine as le
            vp.initialize_rendering(self.editor_bridge, self._preview_scene)
            if vp.view_id:
                vp.editor_bridge.set_view_mode(vp.view_id, le.ViewMode.View2D)
        except Exception as exc:
            import traceback
            print(f"UI Theme: failed to start engine preview: {exc}")
            traceback.print_exc()

    def _rebuild_preview_scene_theme(self) -> None:
        """Point every preview control's `theme` property at the current theme.

        Used after the first save (when an empty theme_path becomes set) and
        after each load so the live preview tracks the theme being edited.
        """
        if not self._preview_controls or not self.editor_bridge:
            return
        theme_res = self._preview_theme_res()
        if not theme_res:
            return
        for record in self._preview_controls:
            comp = record.get("comp")
            if comp is None:
                continue
            try:
                self.editor_bridge.set_component_property_direct(
                    comp, "theme", json.dumps(theme_res))
            except Exception as exc:
                print(f"UI Theme: failed to update preview theme: {exc}")

    def _teardown_preview(self) -> None:
        """Stop and release the embedded preview viewport and its scene."""
        # reject()/Escape/Close route through done() while [X] routes through
        # closeEvent; guard so a second call can't re-save an emptied layout.
        if getattr(self, "_preview_torn_down", False):
            return
        self._preview_torn_down = True
        # Persist the final layout (a safety net; drags also save as they happen)
        # while the view is still alive, then restore the bridge's undo recording.
        if self._use_engine_preview:
            self._save_preview_layout()
        self._set_undo_recording(True)
        if self._preview_viewport is not None:
            try:
                self._preview_viewport.cleanup()
            except Exception as exc:
                print(f"UI Theme: preview cleanup failed: {exc}")
        self._preview_viewport = None
        self._preview_scene = None
        self._preview_controls = []
        self._preview_selected_node = None

    # ------------------------------------------- preview editing & persistence

    def _set_undo_recording(self, enabled: bool) -> None:
        """Toggle the bridge's undo recording, tracking our suppression state.

        Tolerates an engine that predates the `set_undo_recording_enabled` hook
        (the feature still works; gizmo drags merely record undo until rebuilt).
        """
        if not self.editor_bridge:
            return
        try:
            self.editor_bridge.set_undo_recording_enabled(bool(enabled))
            self._undo_recording_suppressed = not enabled
        except AttributeError:
            pass
        except Exception as exc:
            print(f"UI Theme: set_undo_recording_enabled failed: {exc}")

    def _build_add_menu(self) -> QMenu:
        """Build the Add Control menu from every UI component the engine exposes."""
        menu = QMenu(self)
        grouped: Dict[str, List[str]] = {}
        try:
            for info in self.editor_bridge.get_component_types():
                sub = str(getattr(info, "subcategory", "") or "")
                if not sub.startswith("UI"):
                    continue
                grouped.setdefault(sub, []).append(str(info.type_name))
        except Exception as exc:
            print(f"UI Theme: could not enumerate component types: {exc}")

        if not grouped:
            # Fallback: the themed catalog still covers the meaningful controls.
            for type_name in TYPE_CATALOG.keys():
                action = menu.addAction(type_name)
                action.triggered.connect(
                    lambda _checked=False, tn=type_name: self._on_add_control(tn))
            return menu

        for sub in sorted(grouped.keys()):
            if sub.startswith("UI/"):
                label = sub[len("UI/"):]
            else:
                label = "General" if sub == "UI" else sub
            submenu = menu.addMenu(label)
            for type_name in sorted(set(grouped[sub])):
                action = submenu.addAction(type_name)
                action.triggered.connect(
                    lambda _checked=False, tn=type_name: self._on_add_control(tn))
        return menu

    def _default_props_for(self, type_name: str):
        """Default (width, height, extra_props) for a freshly added control."""
        container_types = {
            "Panel", "ColorRect", "NineSlicePanel", "Container", "ScrollContainer",
            "TabContainer", "PaddingContainer", "CenterContainer",
            "HorizontalContainer", "VerticalContainer", "GridContainer",
            "DockContainer", "Stack", "Wrap",
            "SplitContainer", "AspectRatioContainer",
        }
        list_types = {"ItemList", "Tree", "Dropdown", "PopupMenu"}
        text_types = {"Button", "ToggleButton", "TextureButton", "Checkbox",
                      "RadioButton", "Label", "RichTextLabel"}
        if type_name in container_types:
            width, height = 240.0, 120.0
        else:
            width, height = 170.0, 44.0
        extra: Dict[str, Any] = {}
        if type_name in text_types:
            extra["text"] = type_name
        elif type_name in list_types:
            extra["items"] = "Item 1\nItem 2\nItem 3"
        if type_name in ("LineEdit", "TextEdit"):
            extra["placeholder"] = "Editable text"
        return width, height, extra

    def _unique_preview_name(self, type_name: str) -> str:
        """A control node name unique among the current preview controls."""
        existing = {rec.get("name") for rec in self._preview_controls}
        base = f"Preview{type_name}"
        name = base
        n = 1
        while name in existing:
            name = f"{base}{n}"
            n += 1
        return name

    def _on_add_control(self, type_name: str) -> None:
        """Drop a new UI control of `type_name` into the live preview scene."""
        if not (self._use_engine_preview and self._preview_scene and self.editor_bridge):
            return
        try:
            root = self._preview_scene.get_root()
        except Exception as exc:
            print(f"UI Theme: cannot add control, no preview root: {exc}")
            return
        # Cascade successive adds so they don't stack exactly on top of each other.
        n = len(self._preview_controls)
        x = 60.0 + (n % 8) * 26.0
        y = 60.0 + (n % 8) * 26.0
        width, height, extra = self._default_props_for(type_name)
        name = self._unique_preview_name(type_name)
        record = self._add_preview_control(root, type_name, name, x, y,
                                           width, height, extra_props=extra)
        if record is None:
            QMessageBox.warning(self, "Add Control",
                                f"Could not add a '{type_name}' control to the preview.")
            return
        self._save_preview_layout()

    def _on_preview_node_selected(self, node) -> None:
        """Track the control picked in the preview so it can be removed."""
        self._preview_selected_node = node

    def _on_remove_selected_control(self) -> None:
        """Remove the currently selected control from the preview scene."""
        node = self._preview_selected_node
        if node is None or not self._preview_scene:
            QMessageBox.information(self, "Remove Control",
                                    "Select a control in the preview first.")
            return
        try:
            sel_name = node.get_name()
        except Exception:
            return
        record = next((r for r in self._preview_controls
                       if r.get("name") == sel_name), None)
        if record is None:
            return
        try:
            self._preview_scene.get_root().remove_child(record["node"])
        except Exception as exc:
            print(f"UI Theme: failed to remove preview control: {exc}")
        try:
            self._preview_controls.remove(record)
        except ValueError:
            pass
        self._preview_selected_node = None
        # Drop the viewport's selection so the gizmo doesn't target a dead node.
        vp = self._preview_viewport
        if vp is not None and getattr(vp, "view_id", None):
            try:
                self.editor_bridge.set_selected_nodes(vp.view_id, [])
                vp.selected_nodes = []
            except Exception:
                pass
        self._save_preview_layout()

    def _preview_layout_path(self) -> str:
        """Absolute path of this project's saved UI-theme preview layout."""
        if not self.project_dir:
            return ""
        return os.path.join(self.project_dir, ".lupine", "uitheme_preview.json")

    def _load_preview_layout(self) -> Optional[List[Dict[str, Any]]]:
        """Load the saved preview layout's control list, or None if absent."""
        path = self._preview_layout_path()
        if not path or not os.path.isfile(path):
            return None
        try:
            with open(path, "r", encoding="utf-8") as fh:
                data = json.load(fh)
        except Exception as exc:
            print(f"UI Theme: failed to read preview layout: {exc}")
            return None
        if not isinstance(data, dict):
            return None
        controls = data.get("controls")
        if not isinstance(controls, list):
            return None
        return controls

    def _read_node_json(self, node, prop: str):
        """Read a node property back from the engine as a parsed JSON value."""
        try:
            raw = self.editor_bridge.get_node_property(node, prop)
            return json.loads(raw) if raw else None
        except Exception:
            return None

    def _read_comp_float(self, comp, prop: str, default: float) -> float:
        """Read a component property back as a float, falling back to `default`."""
        try:
            raw = self.editor_bridge.get_component_property(comp, prop)
            value = json.loads(raw) if raw else None
            return float(value)
        except Exception:
            return float(default)

    def _save_preview_layout(self) -> None:
        """Read each preview control's live transform/size back and persist it."""
        if not self._use_engine_preview or not self.editor_bridge:
            return
        path = self._preview_layout_path()
        if not path:
            return
        controls_out: List[Dict[str, Any]] = []
        for record in self._preview_controls:
            node = record.get("node")
            comp = record.get("comp")
            if node is None or comp is None:
                continue
            entry: Dict[str, Any] = {
                "type": record.get("type", ""),
                "name": record.get("name", ""),
                "extra": record.get("extra", {}),
            }
            pos = self._read_node_json(node, "position")
            entry["x"] = float(pos.get("x", 0.0)) if isinstance(pos, dict) else 0.0
            entry["y"] = float(pos.get("y", 0.0)) if isinstance(pos, dict) else 0.0
            rotation = self._read_node_json(node, "rotation")
            if isinstance(rotation, (int, float)):
                entry["rotation"] = float(rotation)
            scale = self._read_node_json(node, "scale")
            if isinstance(scale, dict):
                entry["scale"] = {"x": float(scale.get("x", 1.0)),
                                  "y": float(scale.get("y", 1.0))}
            entry["width"] = self._read_comp_float(comp, "width",
                                                   record.get("width", 160.0))
            entry["height"] = self._read_comp_float(comp, "height",
                                                    record.get("height", 44.0))
            controls_out.append(entry)
        data = {"lupine_uitheme_preview": 1, "controls": controls_out}
        try:
            Path(path).parent.mkdir(parents=True, exist_ok=True)
            with open(path, "w", encoding="utf-8") as fh:
                json.dump(data, fh, indent=2)
        except Exception as exc:
            print(f"UI Theme: failed to write preview layout: {exc}")

    def closeEvent(self, event) -> None:
        self._teardown_preview()
        super().closeEvent(event)

    def done(self, result) -> None:
        # exec()/Close/Escape route through done() (not closeEvent), so tear the
        # preview down here too. _teardown_preview is idempotent.
        self._teardown_preview()
        super().done(result)

    # ----------------------------------------------------------- serialize

    def _slot_to_json(self, slot: Dict[str, Any]) -> Dict[str, Any]:
        return {
            "key": slot.get("key", ""),
            "name": slot.get("name", ""),
            "color": self._coerce_rgba(slot.get("color")),
        }

    def _serialize_theme(self) -> Dict[str, Any]:
        out: Dict[str, Any] = {"lupine_theme": 1}
        out["name"] = self.name_edit.text().strip() or "Default"
        out["extends"] = self.theme.get("extends", "") or ""

        if self.palette_mode == "external" and self.palette_ext_res:
            out["palette"] = self.palette_ext_res
        elif self.palette_mode == "inline":
            out["palette"] = {"slots": [self._slot_to_json(s) for s in self.palette_slots]}
        # "none" -> palette key omitted.

        variables: Dict[str, float] = {}
        for var in self._variables:
            key = (var.get("key") or "").strip()
            if key:
                try:
                    variables[key] = float(var.get("value", 0.0))
                except (TypeError, ValueError):
                    variables[key] = 0.0
        out["variables"] = variables

        font_roles: Dict[str, Any] = {}
        for role in self._font_roles:
            name = (role.get("role") or "").strip()
            if name:
                try:
                    size = float(role.get("size", 16.0))
                except (TypeError, ValueError):
                    size = 16.0
                font_roles[name] = {"font": role.get("font", ""), "size": size}
        out["font_roles"] = font_roles

        # Theme-level defaults. Mirror the C++ serializer's omission rules so a
        # round-trip stays byte-for-byte equivalent: empty font, non-positive size
        # and unit base scale are all left out.
        default_font = self.default_font_widget.get_value().strip()
        if default_font:
            out["default_font"] = default_font
        try:
            default_font_size = float(self.default_font_size_spin.value())
        except (TypeError, ValueError):
            default_font_size = 0.0
        if default_font_size > 0.0:
            out["default_font_size"] = default_font_size
        try:
            default_base_scale = float(self.default_base_scale_spin.value())
        except (TypeError, ValueError):
            default_base_scale = 1.0
        if abs(default_base_scale - 1.0) > 1e-9:
            out["default_base_scale"] = default_base_scale

        types_out: Dict[str, Any] = {}
        for type_name, type_data in self._types().items():
            if not isinstance(type_data, dict):
                continue
            type_out: Dict[str, Any] = {}
            if type_data.get("extends"):
                type_out["extends"] = type_data["extends"]
            for category in ("colors", "constants", "vec2s", "bools", "fonts", "images"):
                cat = type_data.get(category)
                if isinstance(cat, dict) and cat:
                    type_out[category] = cat
            # Preserve any existing stylebox definitions verbatim.
            styleboxes = type_data.get("styleboxes")
            if isinstance(styleboxes, dict) and styleboxes:
                type_out["styleboxes"] = styleboxes
            if type_out:
                types_out[type_name] = type_out
        out["types"] = types_out
        return out

    def _palette_to_json(self) -> Dict[str, Any]:
        return {
            "lupine_palette": 1,
            "name": self.palette_name or "Palette",
            "slots": [self._slot_to_json(s) for s in self.palette_slots],
        }

    # -------------------------------------------------------------- saving

    def _engine_reload(self, res_path: str) -> None:
        if not self.editor_bridge:
            return
        try:
            self.editor_bridge.reload_theme(res_path)
        except TypeError:
            try:
                self.editor_bridge.reload_theme()
            except Exception as exc:
                print(f"UI Theme: reload_theme failed: {exc}")
        except Exception as exc:
            print(f"UI Theme: reload_theme failed: {exc}")

    def _do_save(self, show_info: bool) -> bool:
        abs_path = self.theme_path
        if not abs_path:
            name = self.name_edit.text().strip() or "theme"
            start = str(Path(self.project_dir) / f"{name}.uitheme") if self.project_dir \
                else f"{name}.uitheme"
            path, _ = QFileDialog.getSaveFileName(self, "Save UI Theme", start,
                                                  "UI Theme (*.uitheme)")
            if not path:
                return False
            if not path.lower().endswith(".uitheme"):
                path += ".uitheme"
            abs_path = path

        theme_obj = self._serialize_theme()
        try:
            Path(abs_path).parent.mkdir(parents=True, exist_ok=True)
            with open(abs_path, "w", encoding="utf-8") as fh:
                json.dump(theme_obj, fh, indent=2)
        except Exception as exc:
            QMessageBox.critical(self, "Save", f"Failed to write theme:\n{exc}")
            return False
        self.theme_path = abs_path

        if self.palette_mode == "external" and self.palette_ext_res:
            pal_abs = self._res_to_abs(self.palette_ext_res)
            try:
                Path(pal_abs).parent.mkdir(parents=True, exist_ok=True)
                with open(pal_abs, "w", encoding="utf-8") as fh:
                    json.dump(self._palette_to_json(), fh, indent=2)
            except Exception as exc:
                QMessageBox.critical(self, "Save", f"Failed to write palette:\n{exc}")
                return False

        self._engine_reload(self._abs_to_res(abs_path))
        # If this was the first save (theme_path was previously empty) the
        # preview controls were built without a `theme` binding; repoint them at
        # the now-saved path so the live preview tracks the edited theme.
        self._rebuild_preview_scene_theme()
        self.theme_changed.emit(abs_path)
        if show_info:
            QMessageBox.information(self, "UI Theme", "UI theme saved.")
        return True

    def _on_save(self) -> None:
        self._do_save(show_info=True)

    def _on_apply(self) -> None:
        self._do_save(show_info=False)

    def _on_import(self) -> None:
        start = self.project_dir or ""
        path, _ = QFileDialog.getOpenFileName(self, "Import UI Theme", start,
                                              "UI Theme (*.uitheme)")
        if not path:
            return
        self._load_theme(path)
        self._refresh_all()

    def _on_export(self) -> None:
        name = self.name_edit.text().strip() or "theme"
        start = str(Path(self.project_dir) / f"{name}.uitheme") if self.project_dir \
            else f"{name}.uitheme"
        path, _ = QFileDialog.getSaveFileName(self, "Export UI Theme", start,
                                              "UI Theme (*.uitheme)")
        if not path:
            return
        if not path.lower().endswith(".uitheme"):
            path += ".uitheme"
        try:
            with open(path, "w", encoding="utf-8") as fh:
                json.dump(self._serialize_theme(), fh, indent=2)
        except Exception as exc:
            QMessageBox.critical(self, "Export", f"Failed to export theme:\n{exc}")
            return
        QMessageBox.information(self, "Export", f"Exported to:\n{path}")
