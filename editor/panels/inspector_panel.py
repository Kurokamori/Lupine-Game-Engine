"""
Inspector Panel
Display and edit properties of selected objects
"""

from PyQt6.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QLabel, QLineEdit,
                             QPushButton, QScrollArea, QGroupBox, QSpinBox, QDoubleSpinBox,
                             QCheckBox, QComboBox, QColorDialog, QFileDialog, QFrame,
                             QSizePolicy, QMessageBox, QPlainTextEdit, QSlider)
from PyQt6.QtCore import Qt, pyqtSignal, QTimer, QSize
from PyQt6.QtGui import QColor, QPixmap
from .base_panel import EditorPanel
from dialogs import AddComponentDialog
import lupine_engine as le
import json
import re
import math
import sys
from pathlib import Path
import os

# Add parent directory to path to import theme
sys.path.insert(0, str(Path(__file__).parent.parent))
from theme import get_theme_manager
from editor.widgets.inspector_style import (
    InspectorCard, tokens as inspector_tokens, make_property_label,
    reset_button_qss, style_icon_button, with_alpha, mix,
)
from editor.widgets.inspector_icons import icon as inspector_icon

try:
    from editor.widgets import custom_widget_registry
except ImportError:
    from widgets import custom_widget_registry

try:
    from editor.widgets import asset_drag
except ImportError:
    from widgets import asset_drag


# PropertyUsageFlags bitmask (mirrors core/PropertyDescriptor.hpp). Serialized into
# property metadata under the "usage" key.
USAGE_READONLY = 1 << 0
USAGE_HIDDEN = 1 << 1
USAGE_NO_SERIALIZE = 1 << 2
USAGE_REQUIRED = 1 << 3
USAGE_UNIQUE = 1 << 4
USAGE_EXPERIMENTAL = 1 << 5
USAGE_ADVANCED = 1 << 6

# Extended PropertyHintType values (appended after Layers3D=10 in PropertyDescriptor.hpp).
HINT_NODE_TYPE = 11
HINT_ARCHETYPE_TYPE = 12
HINT_SCRIPT_CLASS = 13
HINT_FLAGS = 14
HINT_EXP_EASING = 15


# Property groups contributed by the UIControl base layer (shared sizing, anchoring,
# alignment and theming). For UI components these are pushed below the component's own
# property groups so a control's individual properties (e.g. a Label's Text) appear
# before the shared alignment/anchor controls. The order here is the order they are
# shown in, after all component-specific groups.
UICONTROL_BASE_GROUP_ORDER = ["Layout", "Size Flags", "Transform", "Rendering", "Theme"]

# Logical display order for the WorldEnvironment post-processing groups. The master
# "Post Processing" toggle group comes first, then each effect in pipeline order, with
# the backend-orientation override last. Groups not listed keep their normal ordering.
POSTPROCESS_GROUP_ORDER = [
    "Post Processing",
    "Bloom",
    "SSAO",
    "Color Grading",
    "Vignette",
    "Chromatic Aberration",
    "Film Grain",
    "Overlay",
    "Advanced",
]


def order_component_groups(group_names, is_uicontrol: bool):
    """Return group names in display order.

    Post-processing groups (when present) are placed first in the fixed logical
    order defined by POSTPROCESS_GROUP_ORDER. Remaining component-specific groups
    follow alphabetically; for UIControl-derived components the shared base-layer
    groups (Layout/Size Flags/Transform/Rendering/Theme) are appended afterwards in
    UICONTROL_BASE_GROUP_ORDER.
    """
    def pp_rank(name):
        return POSTPROCESS_GROUP_ORDER.index(name) if name in POSTPROCESS_GROUP_ORDER else None

    if not is_uicontrol:
        def sort_key(name):
            rank = pp_rank(name)
            if rank is not None:
                return (0, rank, name)
            return (1, 0, name)
        return sorted(group_names, key=sort_key)

    def sort_key(name):
        rank = pp_rank(name)
        if rank is not None:
            return (0, rank, name)
        if name in UICONTROL_BASE_GROUP_ORDER:
            return (2, UICONTROL_BASE_GROUP_ORDER.index(name), name)
        return (1, 0, name)

    return sorted(group_names, key=sort_key)


def order_group_properties(props):
    """Order properties within a group so the section's enable toggle leads.

    Any boolean-style "...Enabled" property is shown first (stable, preserving the
    existing relative order of the rest). This makes each effect's master toggle the
    first control in its group.
    """
    return sorted(props, key=lambda item: 0 if str(item[0]).endswith("Enabled") else 1)


def order_properties_by_declaration(properties, metadata):
    """Return (name, value) pairs in their C++ declaration order.

    Components register properties in a deliberate order; that order is carried
    through serialization as an "order" index on each property's metadata (the JSON
    object itself re-sorts keys alphabetically, so the index is the only surviving
    record of declaration order). Properties lacking an index keep their incoming
    order and follow those that have one. Custom group/section sorting downstream is
    applied on top of this ordering, so it remains untouched.
    """
    items = list(properties.items())

    def order_key(indexed_item):
        position, (prop_name, _prop_value) = indexed_item
        meta = metadata.get(prop_name) if metadata else None
        order = meta.get("order") if isinstance(meta, dict) else None
        if isinstance(order, int):
            return (0, order)
        return (1, position)

    return [item for _, item in sorted(enumerate(items), key=order_key)]


# Global project root for path conversion (set by main editor)
_project_root: str = ""


def set_project_root(root: str):
    """Set the project root directory for res:// path conversion"""
    global _project_root
    if root and root.endswith(".lupine"):
        root = os.path.dirname(root)
    _project_root = os.path.normpath(root) if root else ""


def get_project_root() -> str:
    """Get the project root directory"""
    return _project_root


def convert_to_res_path(absolute_path: str) -> str:
    """
    Convert an absolute file path to a res:// path.

    Args:
        absolute_path: The absolute path to convert

    Returns:
        res:// path if the file is within the project, original path otherwise
    """
    if not absolute_path:
        return ""

    # Already a res:// path
    if absolute_path.startswith("res://"):
        return absolute_path

    # Try to use C++ AssetDatabase.to_resource_path() first - it's more reliable
    try:
        asset_db = le.AssetDatabase.get_instance()
        if asset_db.is_initialized():
            res_path = asset_db.to_resource_path(absolute_path)
            if res_path:
                return res_path
    except Exception as e:
        print(f"[convert_to_res_path] AssetDatabase error: {e}")

    # Fallback: manual conversion
    abs_path = os.path.normpath(absolute_path)
    project_root = _project_root

    if not project_root:
        # Try to get from AssetDatabase
        try:
            asset_db = le.AssetDatabase.get_instance()
            if asset_db.is_initialized():
                project_root = asset_db.get_project_root()
        except Exception:
            pass

    if not project_root:
        # Can't convert without project root - return original
        return absolute_path

    project_root = os.path.normpath(project_root)

    # Check if path is within project
    try:
        rel_path = os.path.relpath(abs_path, project_root)

        # If it starts with ".." it's outside the project
        if rel_path.startswith(".."):
            return absolute_path

        # Convert to forward slashes and add res:// prefix
        res_path = "res://" + rel_path.replace("\\", "/")
        return res_path

    except ValueError:
        # Different drives on Windows
        return absolute_path


def convert_from_res_path(res_path: str) -> str:
    """
    Convert a res:// path to an absolute file path.

    Args:
        res_path: The res:// path (or absolute path) to convert

    Returns:
        Absolute path if resolvable, original string otherwise
    """
    if not res_path:
        return ""

    if not res_path.startswith("res://"):
        return res_path

    try:
        asset_db = le.AssetDatabase.get_instance()
        if asset_db.is_initialized():
            abs_path = asset_db.resolve_asset(res_path)
            if abs_path:
                return abs_path
    except Exception as e:
        print(f"[convert_from_res_path] AssetDatabase error: {e}")

    project_root = _project_root
    if not project_root:
        try:
            asset_db = le.AssetDatabase.get_instance()
            if asset_db.is_initialized():
                project_root = asset_db.get_project_root()
        except Exception:
            pass

    if not project_root:
        return res_path

    relative = res_path[len("res://"):]
    return os.path.normpath(os.path.join(project_root, relative))


SCRIPT_FILE_EXTENSIONS = ('.lua', '.py', '.rb')


def format_property_name(name: str) -> str:
    """Convert camelCase or snake_case to Title Case with spaces.

    Examples:
        'castShadow' -> 'Cast Shadow'
        'doubleSided' -> 'Double Sided'
        'z_index' -> 'Z Index'
    """
    # Replace underscores with spaces
    name = name.replace('_', ' ')

    # Insert space before uppercase letters (camelCase)
    name = re.sub(r'([a-z])([A-Z])', r'\1 \2', name)

    # Capitalize each word
    name = ' '.join(word.capitalize() for word in name.split())

    return name


def get_theme_spacing():
    """Get theme spacing values for inspector widgets"""
    theme = get_theme_manager().get_current_theme()
    if theme:
        return theme.colors.inspector_vertical_spacing, theme.colors.inspector_horizontal_padding
    return 8, 6  # defaults


class CollapsibleGroupBox(QWidget):
    """A collapsible sub-section inside a component card.

    Section titles read as quiet uppercase "eyebrows" (structure as signposting, not
    decoration); content sits behind a hairline indent guide so grouped properties feel
    nested without adding heavy borders.
    """

    def __init__(self, title, parent=None):
        super().__init__(parent)
        self.title = title
        self.is_collapsed = False
        t = inspector_tokens()
        self._tokens = t

        main_layout = QVBoxLayout()
        main_layout.setContentsMargins(0, 4, 0, 0)
        main_layout.setSpacing(0)

        # Eyebrow header: chevron + uppercase, tracked label
        self.header_button = QPushButton()
        self.header_button.setCheckable(True)
        self.header_button.setChecked(True)
        self.header_button.setCursor(Qt.CursorShape.PointingHandCursor)
        self.header_button.clicked.connect(self.toggle_collapsed)
        self.update_header_text()
        self.header_button.setStyleSheet(f"""
            QPushButton {{
                background-color: transparent;
                border: none;
                border-bottom: 1px solid {with_alpha(t.border, 0.5)};
                padding: 4px 2px 5px 2px;
                text-align: left;
                font-size: 9px;
                font-weight: bold;
                letter-spacing: 1px;
                color: {t.text_secondary};
            }}
            QPushButton:hover {{
                color: {t.text_primary};
                border-bottom-color: {t.accent};
            }}
        """)
        main_layout.addWidget(self.header_button)

        # Content widget, inset behind a faint indent guide
        self.content_widget = QWidget()
        self.content_widget.setObjectName("sectionContent")
        self.content_layout = QVBoxLayout()
        self.content_layout.setContentsMargins(8, 4, 0, 2)
        self.content_layout.setSpacing(t.row_v_pad)
        self.content_widget.setLayout(self.content_layout)
        self.content_widget.setStyleSheet(
            f"QWidget#sectionContent {{ border-left: 1px solid {with_alpha(t.border, 0.45)}; }}")
        main_layout.addWidget(self.content_widget)

        self.setLayout(main_layout)

    def update_header_text(self):
        """Update the header text with collapse indicator"""
        arrow = "▾" if not self.is_collapsed else "▸"
        self.header_button.setText(f"{arrow}  {self.title.upper()}")

    def toggle_collapsed(self):
        """Toggle the collapsed state"""
        self.is_collapsed = not self.is_collapsed
        self.content_widget.setVisible(not self.is_collapsed)
        self.update_header_text()

    def add_widget(self, widget):
        """Add a widget to the content area"""
        self.content_layout.addWidget(widget)


class PropertyWidget(QWidget):
    """Base class for property editing widgets.

    Provides the shared row chrome used by every editor: a full-width hover tint and a
    revert affordance that stays hidden until the row is hovered (so rows read clean at
    rest). Subclasses build their controls in `setup_ui()` and use `_make_label()` /
    `_create_reset_button()` for a consistent, theme-driven look.
    """
    value_changed = pyqtSignal(object)  # new value

    def __init__(self, property_name, default_value=None, parent=None):
        super().__init__(parent)
        self.property_name = property_name
        self.default_value = default_value
        self.reset_button = None   # Inner revert glyph (revealed on hover)
        self.reset_holder = None   # Fixed-width slot that reserves space for the glyph
        self.setup_ui()
        self._install_row_chrome()

    def _install_row_chrome(self):
        """Apply the shared hover background; safe for composite widgets too."""
        t = inspector_tokens()
        self.setObjectName("propRow")
        self.setAttribute(Qt.WidgetAttribute.WA_StyledBackground, True)
        self.setStyleSheet(
            f"QWidget#propRow {{ background-color: transparent; border-radius: 4px; }}"
            f"QWidget#propRow:hover {{ background-color: {t.row_hover}; }}"
        )

    def set_mixed(self, mixed=True):
        """Flag this row as holding multiple values across a multi-node selection.

        Adds an accent spine and an explanatory tooltip so it's clear the displayed
        value is one of several and that editing it rewrites all of them. Cleared by
        passing ``mixed=False`` (restores the default row chrome)."""
        self._mixed = bool(mixed)
        if mixed:
            t = inspector_tokens()
            self.setToolTip(
                "Multiple values across the selection — editing applies to all selected nodes")
            self.setStyleSheet(
                f"QWidget#propRow {{ background-color: transparent; border-radius: 4px;"
                f" border-left: 2px solid {t.accent}; }}"
                f"QWidget#propRow:hover {{ background-color: {t.row_hover};"
                f" border-left: 2px solid {t.accent}; }}"
            )
        else:
            self.setToolTip("")
            self._install_row_chrome()

    def enterEvent(self, event):
        if self.reset_button is not None:
            self.reset_button.setVisible(True)
        super().enterEvent(event)

    def leaveEvent(self, event):
        if self.reset_button is not None:
            self.reset_button.setVisible(False)
        super().leaveEvent(event)

    def setup_ui(self):
        """Setup the widget UI - override in subclasses"""
        pass

    def set_value(self, value):
        """Set the widget value - override in subclasses"""
        pass

    def get_value(self):
        """Get the widget value - override in subclasses"""
        return None

    def reset_to_default(self):
        """Reset the widget to its default value - override in subclasses"""
        if self.default_value is not None:
            self.set_value(self.default_value)
            # Emit value_changed to propagate the change to the backend
            self.value_changed.emit(self.default_value)

    def _make_label(self, text=None, tooltip=None):
        """Create the themed, fixed-width left-column label for this property row."""
        return make_property_label(text if text is not None else self.property_name, tooltip)

    def _create_reset_button(self):
        """Create the revert affordance for this row.

        Returns a fixed-width holder so the row's controls stay aligned whether or not a
        revert glyph is present. The glyph itself only exists when the property has a known
        default, and is hidden until the row is hovered.
        """
        t = inspector_tokens()
        holder = QWidget()
        holder.setFixedSize(t.control_height, t.control_height)
        holder.setStyleSheet("background: transparent;")
        holder_layout = QHBoxLayout(holder)
        holder_layout.setContentsMargins(0, 0, 0, 0)
        holder_layout.setSpacing(0)
        self.reset_holder = holder

        if self.default_value is not None:
            reset_btn = QPushButton()
            reset_btn.setIcon(inspector_icon("revert", t.text_secondary))
            reset_btn.setIconSize(QSize(14, 14))
            reset_btn.setFixedSize(t.control_height, t.control_height)
            reset_btn.setCursor(Qt.CursorShape.PointingHandCursor)
            reset_btn.setToolTip("Reset to default value")
            reset_btn.setStyleSheet(reset_button_qss(t))
            reset_btn.clicked.connect(self.reset_to_default)
            reset_btn.setVisible(False)  # Revealed on row hover
            holder_layout.addWidget(reset_btn)
            self.reset_button = reset_btn

        return holder


class NestedStructPropertyWidget(PropertyWidget):
    """Edits a Dictionary-backed inline struct as a foldout mini-inspector.

    The struct's field schema (a list of serialized PropertyDescriptors) comes from a
    script ``@struct`` declaration; each field is rendered with the panel's own widget
    factory so it benefits from the same type/hint handling as any other property.
    The aggregate value is a dict keyed by field name.
    """

    def __init__(self, property_name, object_schema, value, panel, type_name="", parent=None):
        self._schema = object_schema or []
        self._value = dict(value) if isinstance(value, dict) else {}
        self._panel = panel
        self._type_name = type_name or ""
        self._child_widgets = {}
        super().__init__(property_name, None, parent)

    def setup_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)

        title = format_property_name(self.property_name)
        if self._type_name:
            title = f"{title}  ({self._type_name})"
        group = CollapsibleGroupBox(title)

        for field in self._schema:
            if not isinstance(field, dict):
                continue
            field_name = field.get("name")
            if not field_name:
                continue
            field_default = field.get("default", field.get("defaultValue"))
            field_value = self._value.get(field_name, field_default)
            child = self._panel._create_property_widget(field_name, field_value, field)
            if child is None:
                continue
            child.value_changed.connect(
                lambda v, n=field_name: self._on_field_changed(n, v))
            group.add_widget(child)
            self._child_widgets[field_name] = child
            if field_name not in self._value:
                self._value[field_name] = field_default

        layout.addWidget(group)

    def _on_field_changed(self, field_name, value):
        self._value[field_name] = value
        self.value_changed.emit(dict(self._value))

    def set_value(self, value):
        if not isinstance(value, dict):
            return
        self._value = dict(value)
        for field_name, child in self._child_widgets.items():
            if field_name in self._value:
                child.blockSignals(True)
                child.set_value(self._value[field_name])
                child.blockSignals(False)

    def get_value(self):
        return dict(self._value)


class FilteredReferencePropertyWidget(PropertyWidget):
    """Picks a reference (node path or archetype instance path) filtered by class.

    Backs the NodeType / ScriptClass / ArchetypeType hints. The candidate list is
    pulled from the editor bridge; the stored value is the selected path string.
    Falls back to a free-text path if the bridge is unavailable.
    """

    KIND_NODE = "node"
    KIND_ARCHETYPE = "archetype"

    def __init__(self, property_name, default_value, panel, kind, class_name, parent=None):
        self._panel = panel
        self._kind = kind
        self._class_name = class_name or ""
        super().__init__(property_name, default_value, parent)

    def setup_ui(self):
        layout = QHBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(6)
        layout.addWidget(self._make_label())

        self._combo = QComboBox()
        self._combo.setEditable(True)
        self._populate()
        self._combo.currentIndexChanged.connect(self._on_changed)
        self._combo.lineEdit().editingFinished.connect(
            lambda: self.value_changed.emit(self.get_value()))
        layout.addWidget(self._combo, 1)
        layout.addWidget(self._create_reset_button())

    def _candidates(self):
        bridge = getattr(self._panel, "editor_bridge", None)
        if bridge is None:
            return []
        try:
            if self._kind == self.KIND_ARCHETYPE:
                raw = bridge.find_archetype_instances(self._class_name)
            else:
                raw = bridge.find_nodes_by_class(self._class_name)
            entries = json.loads(raw) if isinstance(raw, str) else (raw or [])
        except Exception:
            return []
        items = []
        for entry in entries:
            if not isinstance(entry, dict):
                continue
            path = entry.get("path", "")
            if self._kind == self.KIND_ARCHETYPE:
                label = entry.get("display") or path
            else:
                label = entry.get("name") or path
                if entry.get("type"):
                    label = f"{label}  [{entry['type']}]"
            items.append((label, path))
        return items

    def _populate(self):
        self._combo.blockSignals(True)
        self._combo.clear()
        self._combo.addItem("(none)", "")
        for label, path in self._candidates():
            self._combo.addItem(label, path)
        self._combo.blockSignals(False)

    def _on_changed(self, _index):
        self.value_changed.emit(self.get_value())

    def set_value(self, value):
        path = value if isinstance(value, str) else ""
        self._combo.blockSignals(True)
        index = self._combo.findData(path)
        if index < 0 and path:
            self._combo.addItem(path, path)
            index = self._combo.findData(path)
        self._combo.setCurrentIndex(index if index >= 0 else 0)
        self._combo.blockSignals(False)

    def get_value(self):
        data = self._combo.currentData()
        if data is not None and data != "":
            return data
        text = self._combo.currentText().strip()
        return "" if text == "(none)" else text


class ThemeVariationPropertyWidget(PropertyWidget):
    """Picks a UI theme type variation (the ``themeTypeVariation`` string property).

    The candidate list is sourced from the project's ``.uitheme`` assets: every type
    entry whose ``extends`` names one of the component's theme types (its own type and
    ancestors) is a variation of that type. A blank ``(none)`` option clears the value.
    The combo stays editable so a name can still be typed when no matching variation has
    been authored yet (or the theme cannot be read), and the stored value is the string.
    """

    def __init__(self, property_name, default_value, panel, base_types, parent=None):
        self._panel = panel
        self._base_types = [str(b) for b in (base_types or []) if b]
        super().__init__(property_name, default_value, parent)

    def setup_ui(self):
        layout = QHBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(6)
        layout.addWidget(self._make_label())

        self._combo = QComboBox()
        self._combo.setEditable(True)
        self._populate()
        self._combo.currentIndexChanged.connect(self._on_changed)
        self._combo.lineEdit().editingFinished.connect(
            lambda: self.value_changed.emit(self.get_value()))
        layout.addWidget(self._combo, 1)
        layout.addWidget(self._create_reset_button())

    def _variations(self):
        discover = getattr(self._panel, "_discover_theme_variations", None)
        if discover is None:
            return []
        try:
            return discover(self._base_types)
        except Exception:
            return []

    def _populate(self):
        self._combo.blockSignals(True)
        self._combo.clear()
        self._combo.addItem("(none)", "")
        for name, base in self._variations():
            label = f"{name}  ({base})" if base else name
            self._combo.addItem(label, name)
        self._combo.blockSignals(False)

    def _on_changed(self, _index):
        self.value_changed.emit(self.get_value())

    def set_value(self, value):
        name = value if isinstance(value, str) else ""
        self._combo.blockSignals(True)
        index = self._combo.findData(name)
        if index < 0 and name:
            self._combo.addItem(name, name)
            index = self._combo.findData(name)
        if index >= 0:
            self._combo.setCurrentIndex(index)
        else:
            self._combo.setCurrentIndex(0)
        self._combo.blockSignals(False)

    def get_value(self):
        data = self._combo.currentData()
        if data:
            return data
        text = self._combo.currentText().strip()
        return "" if text == "(none)" else text


class IntPropertyWidget(PropertyWidget):
    """Widget for editing integer properties"""

    def setup_ui(self):
        layout = QHBoxLayout()
        v_spacing, h_padding = get_theme_spacing()
        layout.setContentsMargins(0, v_spacing // 2, 0, v_spacing // 2)
        layout.setSpacing(h_padding)

        self.label = self._make_label()

        self.spinbox = QSpinBox()
        self.spinbox.setRange(-2147483648, 2147483647)
        self.spinbox.setMinimumWidth(60)
        self.spinbox.setMinimumHeight(28)
        self.spinbox.setMinimumHeight(28)
        self.spinbox.valueChanged.connect(lambda v: self.value_changed.emit(v))
        self.spinbox.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)

        layout.addWidget(self.label)
        layout.addWidget(self.spinbox)
        layout.addWidget(self._create_reset_button())
        self.setLayout(layout)

    def set_value(self, value):
        self.spinbox.blockSignals(True)
        self.spinbox.setValue(int(value))
        self.spinbox.blockSignals(False)

    def get_value(self):
        return self.spinbox.value()


class FloatPropertyWidget(PropertyWidget):
    """Widget for editing float properties"""

    def setup_ui(self):
        layout = QHBoxLayout()
        v_spacing, h_padding = get_theme_spacing()
        layout.setContentsMargins(0, v_spacing // 2, 0, v_spacing // 2)
        layout.setSpacing(h_padding)

        self.label = self._make_label()

        self.spinbox = QDoubleSpinBox()
        self.spinbox.setRange(-1e10, 1e10)
        self.spinbox.setDecimals(3)
        self.spinbox.setSingleStep(0.1)
        self.spinbox.setMinimumWidth(60)
        self.spinbox.setMinimumHeight(28)
        self.spinbox.valueChanged.connect(lambda v: self.value_changed.emit(v))
        self.spinbox.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)

        layout.addWidget(self.label)
        layout.addWidget(self.spinbox)
        layout.addWidget(self._create_reset_button())
        self.setLayout(layout)

    def set_value(self, value):
        self.spinbox.blockSignals(True)
        self.spinbox.setValue(float(value))
        self.spinbox.blockSignals(False)

    def get_value(self):
        return self.spinbox.value()


class BoolPropertyWidget(PropertyWidget):
    """Widget for editing boolean properties"""

    def setup_ui(self):
        layout = QHBoxLayout()
        v_spacing, h_padding = get_theme_spacing()
        layout.setContentsMargins(0, v_spacing // 2, 0, v_spacing // 2)

        self.checkbox = QCheckBox(self.property_name)
        self.checkbox.setMinimumHeight(28)
        self.checkbox.toggled.connect(lambda checked: self.value_changed.emit(checked))

        layout.addWidget(self.checkbox)
        layout.addStretch()
        self.setLayout(layout)

    def set_value(self, value):
        self.checkbox.blockSignals(True)
        self.checkbox.setChecked(bool(value))
        self.checkbox.blockSignals(False)

    def get_value(self):
        return self.checkbox.isChecked()


class EnumPropertyWidget(PropertyWidget):
    """Widget for editing enum properties"""

    def __init__(self, property_name, enum_values=None, default_value=None, parent=None):
        self.enum_values = enum_values or []
        super().__init__(property_name, default_value, parent)

    def setup_ui(self):
        layout = QHBoxLayout()
        v_spacing, h_padding = get_theme_spacing()
        layout.setContentsMargins(0, v_spacing // 2, 0, v_spacing // 2)
        layout.setSpacing(h_padding)

        self.label = self._make_label()

        self.combobox = QComboBox()
        self.combobox.setMinimumHeight(28)
        self.combobox.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)

        # Add enum values to combobox
        if self.enum_values:
            self.combobox.addItems(self.enum_values)

        self.combobox.currentIndexChanged.connect(lambda index: self.value_changed.emit(index))

        layout.addWidget(self.label)
        layout.addWidget(self.combobox)
        layout.addWidget(self._create_reset_button())
        self.setLayout(layout)

    def set_value(self, value):
        self.combobox.blockSignals(True)
        # Value is the enum index
        if isinstance(value, int) and 0 <= value < self.combobox.count():
            self.combobox.setCurrentIndex(value)
        self.combobox.blockSignals(False)

    def get_value(self):
        return self.combobox.currentIndex()


class StringPropertyWidget(PropertyWidget):
    """Widget for editing string properties"""

    def setup_ui(self):
        layout = QHBoxLayout()
        v_spacing, h_padding = get_theme_spacing()
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(h_padding)

        self.label = self._make_label()

        self.line_edit = QLineEdit()
        self.line_edit.setMinimumWidth(60)
        self.line_edit.editingFinished.connect(lambda: self.value_changed.emit(self.line_edit.text()))
        self.line_edit.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)

        layout.addWidget(self.label)
        layout.addWidget(self.line_edit)
        layout.addWidget(self._create_reset_button())
        self.setLayout(layout)

    def set_value(self, value):
        self.line_edit.blockSignals(True)
        self.line_edit.setText(str(value) if value is not None else "")
        self.line_edit.blockSignals(False)

    def get_value(self):
        return self.line_edit.text()


class Vec2PropertyWidget(PropertyWidget):
    """Widget for editing 2D vector properties"""

    def setup_ui(self):
        layout = QHBoxLayout()
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(2)

        self.label = self._make_label()

        self.x_spinbox = QDoubleSpinBox()
        self.x_spinbox.setPrefix("X:")
        self.x_spinbox.setRange(-1e10, 1e10)
        self.x_spinbox.setDecimals(2)
        self.x_spinbox.valueChanged.connect(self._on_value_changed)
        self.x_spinbox.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)

        self.y_spinbox = QDoubleSpinBox()
        self.y_spinbox.setPrefix("Y:")
        self.y_spinbox.setRange(-1e10, 1e10)
        self.y_spinbox.setDecimals(2)
        self.y_spinbox.setMinimumWidth(50)
        self.y_spinbox.valueChanged.connect(self._on_value_changed)
        self.y_spinbox.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)

        layout.addWidget(self.label)
        layout.addWidget(self.x_spinbox)
        layout.addWidget(self.y_spinbox)
        layout.addWidget(self._create_reset_button())
        self.setLayout(layout)

    def _on_value_changed(self):
        self.value_changed.emit((self.x_spinbox.value(), self.y_spinbox.value()))

    def set_value(self, value):
        self.x_spinbox.blockSignals(True)
        self.y_spinbox.blockSignals(True)
        if hasattr(value, '__iter__') and len(value) >= 2:
            self.x_spinbox.setValue(float(value[0]))
            self.y_spinbox.setValue(float(value[1]))
        self.x_spinbox.blockSignals(False)
        self.y_spinbox.blockSignals(False)

    def get_value(self):
        return (self.x_spinbox.value(), self.y_spinbox.value())

    def reset_to_default(self):
        """Override to emit the correct tuple format"""
        if self.default_value is not None:
            self.set_value(self.default_value)
            self.value_changed.emit(self.get_value())


class Vec3PropertyWidget(PropertyWidget):
    """Widget for editing 3D vector properties"""

    def setup_ui(self):
        layout = QHBoxLayout()
        layout.setContentsMargins(0, 3, 0, 3)
        layout.setSpacing(2)

        self.label = self._make_label()

        self.x_spinbox = QDoubleSpinBox()
        self.x_spinbox.setPrefix("X:")
        self.x_spinbox.setRange(-1e10, 1e10)
        self.x_spinbox.setDecimals(2)
        self.x_spinbox.setMinimumWidth(45)
        self.x_spinbox.setMinimumHeight(28)
        self.x_spinbox.valueChanged.connect(self._on_value_changed)
        self.x_spinbox.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)

        self.y_spinbox = QDoubleSpinBox()
        self.y_spinbox.setPrefix("Y:")
        self.y_spinbox.setRange(-1e10, 1e10)
        self.y_spinbox.setDecimals(2)
        self.y_spinbox.setMinimumWidth(45)
        self.y_spinbox.setMinimumHeight(28)
        self.y_spinbox.valueChanged.connect(self._on_value_changed)
        self.y_spinbox.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)

        self.z_spinbox = QDoubleSpinBox()
        self.z_spinbox.setPrefix("Z:")
        self.z_spinbox.setRange(-1e10, 1e10)
        self.z_spinbox.setDecimals(2)
        self.z_spinbox.setMinimumWidth(45)
        self.z_spinbox.setMinimumHeight(28)
        self.z_spinbox.valueChanged.connect(self._on_value_changed)
        self.z_spinbox.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)

        layout.addWidget(self.label)
        layout.addWidget(self.x_spinbox)
        layout.addWidget(self.y_spinbox)
        layout.addWidget(self.z_spinbox)
        layout.addWidget(self._create_reset_button())

        self.setLayout(layout)

    def _on_value_changed(self):
        self.value_changed.emit((self.x_spinbox.value(), self.y_spinbox.value(), self.z_spinbox.value()))

    def set_value(self, value):
        self.x_spinbox.blockSignals(True)
        self.y_spinbox.blockSignals(True)
        self.z_spinbox.blockSignals(True)
        if hasattr(value, '__iter__') and len(value) >= 3:
            self.x_spinbox.setValue(float(value[0]))
            self.y_spinbox.setValue(float(value[1]))
            self.z_spinbox.setValue(float(value[2]))
        self.x_spinbox.blockSignals(False)
        self.y_spinbox.blockSignals(False)
        self.z_spinbox.blockSignals(False)

    def get_value(self):
        return (self.x_spinbox.value(), self.y_spinbox.value(), self.z_spinbox.value())

    def reset_to_default(self):
        """Override to emit the correct tuple format"""
        if self.default_value is not None:
            self.set_value(self.default_value)
            self.value_changed.emit(self.get_value())


class EulerRotationWidget(PropertyWidget):
    """Widget for editing 3D rotation as Euler angles (degrees) with mode toggle."""

    def __init__(self, property_name="Rotation", default_value=None, parent=None):
        self.use_euler = True  # Default to Euler mode
        self.current_quat = {"x": 0.0, "y": 0.0, "z": 0.0, "w": 1.0}  # Store current quaternion
        super().__init__(property_name, default_value, parent)

    def setup_ui(self):
        layout = QVBoxLayout()
        layout.setContentsMargins(0, 3, 0, 3)
        layout.setSpacing(2)

        # Top row: Label on left, reset button and mode toggle button on right
        top_layout = QHBoxLayout()
        top_layout.setSpacing(2)

        self.label = self._make_label()
        top_layout.addWidget(self.label)

        top_layout.addStretch()  # Push buttons to the right

        top_layout.addWidget(self._create_reset_button())

        self.mode_button = QPushButton("Euler (deg)")
        self.mode_button.setFixedWidth(90)
        self.mode_button.setFixedHeight(24)
        self.mode_button.clicked.connect(self._toggle_mode)
        top_layout.addWidget(self.mode_button)

        layout.addLayout(top_layout)

        # Bottom row: X, Y, Z, W spinboxes (W hidden in Euler mode)
        spinbox_layout = QHBoxLayout()
        spinbox_layout.setSpacing(2)
        spinbox_layout.setContentsMargins(0, 0, 0, 0)

        self.x_spinbox = QDoubleSpinBox()
        self.x_spinbox.setPrefix("X:")
        self.x_spinbox.setRange(-360, 360)
        self.x_spinbox.setDecimals(1)
        self.x_spinbox.setMinimumWidth(40)
        self.x_spinbox.setMinimumHeight(28)
        self.x_spinbox.valueChanged.connect(self._on_value_changed)
        self.x_spinbox.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)

        self.y_spinbox = QDoubleSpinBox()
        self.y_spinbox.setPrefix("Y:")
        self.y_spinbox.setRange(-360, 360)
        self.y_spinbox.setDecimals(1)
        self.y_spinbox.setMinimumWidth(40)
        self.y_spinbox.setMinimumHeight(28)
        self.y_spinbox.valueChanged.connect(self._on_value_changed)
        self.y_spinbox.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)

        self.z_spinbox = QDoubleSpinBox()
        self.z_spinbox.setPrefix("Z:")
        self.z_spinbox.setRange(-360, 360)
        self.z_spinbox.setDecimals(1)
        self.z_spinbox.setMinimumWidth(40)
        self.z_spinbox.setMinimumHeight(28)
        self.z_spinbox.valueChanged.connect(self._on_value_changed)
        self.z_spinbox.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)

        self.w_spinbox = QDoubleSpinBox()
        self.w_spinbox.setPrefix("W:")
        self.w_spinbox.setRange(-1, 1)
        self.w_spinbox.setDecimals(3)
        self.w_spinbox.setMinimumWidth(40)
        self.w_spinbox.setMinimumHeight(28)
        self.w_spinbox.valueChanged.connect(self._on_value_changed)
        self.w_spinbox.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)
        self.w_spinbox.setVisible(False)  # Hidden in Euler mode by default

        spinbox_layout.addWidget(self.x_spinbox)
        spinbox_layout.addWidget(self.y_spinbox)
        spinbox_layout.addWidget(self.z_spinbox)
        spinbox_layout.addWidget(self.w_spinbox)

        layout.addLayout(spinbox_layout)
        self.setLayout(layout)

    def _toggle_mode(self):
        """Toggle between Euler and Quaternion display modes."""
        self.use_euler = not self.use_euler
        if self.use_euler:
            self.mode_button.setText("Euler (deg)")
            self.x_spinbox.setRange(-360, 360)
            self.y_spinbox.setRange(-360, 360)
            self.z_spinbox.setRange(-360, 360)
            self.x_spinbox.setDecimals(1)
            self.y_spinbox.setDecimals(1)
            self.z_spinbox.setDecimals(1)
            self.w_spinbox.setVisible(False)
            # Convert current quaternion to Euler
            euler = self._quat_to_euler(self.current_quat)
            self.set_euler_values(euler)
        else:
            self.mode_button.setText("Quaternion")
            self.x_spinbox.setRange(-1, 1)
            self.y_spinbox.setRange(-1, 1)
            self.z_spinbox.setRange(-1, 1)
            self.x_spinbox.setDecimals(3)
            self.y_spinbox.setDecimals(3)
            self.z_spinbox.setDecimals(3)
            self.w_spinbox.setVisible(True)
            # Show all quaternion components (x, y, z, w)
            self.set_quat_xyzw_values(self.current_quat)

    def _quat_to_euler(self, quat):
        """Convert quaternion to Euler angles in degrees (ZYX order)."""
        x, y, z, w = quat["x"], quat["y"], quat["z"], quat["w"]
        
        # Convert to Euler angles (in radians)
        # Using ZYX rotation order (yaw-pitch-roll)
        sinr_cosp = 2 * (w * x + y * z)
        cosr_cosp = 1 - 2 * (x * x + y * y)
        roll = math.atan2(sinr_cosp, cosr_cosp)
        
        sinp = 2 * (w * y - z * x)
        if abs(sinp) >= 1:
            pitch = math.copysign(math.pi / 2, sinp)
        else:
            pitch = math.asin(sinp)
        
        siny_cosp = 2 * (w * z + x * y)
        cosy_cosp = 1 - 2 * (y * y + z * z)
        yaw = math.atan2(siny_cosp, cosy_cosp)
        
        # Convert to degrees
        return (math.degrees(roll), math.degrees(pitch), math.degrees(yaw))

    def _euler_to_quat(self, euler_deg):
        """Convert Euler angles in degrees to quaternion (ZYX order)."""
        roll, pitch, yaw = math.radians(euler_deg[0]), math.radians(euler_deg[1]), math.radians(euler_deg[2])
        
        cy = math.cos(yaw * 0.5)
        sy = math.sin(yaw * 0.5)
        cp = math.cos(pitch * 0.5)
        sp = math.sin(pitch * 0.5)
        cr = math.cos(roll * 0.5)
        sr = math.sin(roll * 0.5)
        
        w = cr * cp * cy + sr * sp * sy
        x = sr * cp * cy - cr * sp * sy
        y = cr * sp * cy + sr * cp * sy
        z = cr * cp * sy - sr * sp * cy
        
        return {"x": x, "y": y, "z": z, "w": w}

    def _on_value_changed(self):
        if self.use_euler:
            # User edited Euler angles - convert to quaternion
            euler = (self.x_spinbox.value(), self.y_spinbox.value(), self.z_spinbox.value())
            self.current_quat = self._euler_to_quat(euler)
        else:
            # User edited quaternion components - read all 4 values
            self.current_quat["x"] = self.x_spinbox.value()
            self.current_quat["y"] = self.y_spinbox.value()
            self.current_quat["z"] = self.z_spinbox.value()
            self.current_quat["w"] = self.w_spinbox.value()
            # Normalize the quaternion
            mag = math.sqrt(self.current_quat["x"]**2 + self.current_quat["y"]**2 + 
                          self.current_quat["z"]**2 + self.current_quat["w"]**2)
            if mag > 0.0001:
                self.current_quat["x"] /= mag
                self.current_quat["y"] /= mag
                self.current_quat["z"] /= mag
                self.current_quat["w"] /= mag
                # Update display with normalized values
                self.set_quat_xyzw_values(self.current_quat)
        
        # Always emit the quaternion
        self.value_changed.emit(self.current_quat)

    def set_euler_values(self, euler):
        """Set spinbox values from Euler angles (degrees)."""
        self.x_spinbox.blockSignals(True)
        self.y_spinbox.blockSignals(True)
        self.z_spinbox.blockSignals(True)
        self.x_spinbox.setValue(float(euler[0]))
        self.y_spinbox.setValue(float(euler[1]))
        self.z_spinbox.setValue(float(euler[2]))
        self.x_spinbox.blockSignals(False)
        self.y_spinbox.blockSignals(False)
        self.z_spinbox.blockSignals(False)

    def set_quat_xyzw_values(self, quat):
        """Set spinbox values from quaternion x, y, z, w components."""
        self.x_spinbox.blockSignals(True)
        self.y_spinbox.blockSignals(True)
        self.z_spinbox.blockSignals(True)
        self.w_spinbox.blockSignals(True)
        self.x_spinbox.setValue(float(quat["x"]))
        self.y_spinbox.setValue(float(quat["y"]))
        self.z_spinbox.setValue(float(quat["z"]))
        self.w_spinbox.setValue(float(quat["w"]))
        self.x_spinbox.blockSignals(False)
        self.y_spinbox.blockSignals(False)
        self.z_spinbox.blockSignals(False)
        self.w_spinbox.blockSignals(False)

    def set_value(self, value):
        """Set value from quaternion dict."""
        if isinstance(value, dict) and all(k in value for k in ("x", "y", "z", "w")):
            self.current_quat = {
                "x": float(value["x"]),
                "y": float(value["y"]),
                "z": float(value["z"]),
                "w": float(value["w"])
            }
            if self.use_euler:
                euler = self._quat_to_euler(self.current_quat)
                self.set_euler_values(euler)
            else:
                self.set_quat_xyzw_values(self.current_quat)

    def get_value(self):
        """Always return quaternion dict."""
        return self.current_quat

    def reset_to_default(self):
        """Override to emit the correct quaternion format"""
        if self.default_value is not None:
            self.set_value(self.default_value)
            # Emit as quaternion dict
            self.value_changed.emit(self.get_value())


class Vec4PropertyWidget(PropertyWidget):
    """Widget for editing 4D vector properties (e.g., quaternions)."""

    def setup_ui(self):
        main_layout = QVBoxLayout()
        main_layout.setContentsMargins(0, 3, 0, 3)
        main_layout.setSpacing(2)

        # First row: label and reset button
        top_layout = QHBoxLayout()
        top_layout.setSpacing(2)

        self.label = self._make_label()
        top_layout.addWidget(self.label)
        top_layout.addStretch()
        top_layout.addWidget(self._create_reset_button())

        main_layout.addLayout(top_layout)

        # Second row: spinboxes
        spinbox_layout = QHBoxLayout()
        spinbox_layout.setSpacing(2)

        self.x_spinbox = QDoubleSpinBox()
        self.x_spinbox.setPrefix("X:")
        self.x_spinbox.setRange(-1e10, 1e10)
        self.x_spinbox.setDecimals(2)
        self.x_spinbox.setMinimumWidth(40)
        self.x_spinbox.setMinimumHeight(28)
        self.x_spinbox.valueChanged.connect(self._on_value_changed)
        self.x_spinbox.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)

        self.y_spinbox = QDoubleSpinBox()
        self.y_spinbox.setPrefix("Y:")
        self.y_spinbox.setRange(-1e10, 1e10)
        self.y_spinbox.setDecimals(2)
        self.y_spinbox.setMinimumWidth(40)
        self.y_spinbox.setMinimumHeight(28)
        self.y_spinbox.valueChanged.connect(self._on_value_changed)
        self.y_spinbox.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)

        self.z_spinbox = QDoubleSpinBox()
        self.z_spinbox.setPrefix("Z:")
        self.z_spinbox.setRange(-1e10, 1e10)
        self.z_spinbox.setDecimals(2)
        self.z_spinbox.setMinimumWidth(40)
        self.z_spinbox.setMinimumHeight(28)
        self.z_spinbox.valueChanged.connect(self._on_value_changed)
        self.z_spinbox.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)

        self.w_spinbox = QDoubleSpinBox()
        self.w_spinbox.setPrefix("W:")
        self.w_spinbox.setRange(-1e10, 1e10)
        self.w_spinbox.setDecimals(2)
        self.w_spinbox.setMinimumWidth(40)
        self.w_spinbox.setMinimumHeight(28)
        self.w_spinbox.valueChanged.connect(self._on_value_changed)
        self.w_spinbox.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)

        spinbox_layout.addWidget(self.x_spinbox)
        spinbox_layout.addWidget(self.y_spinbox)
        spinbox_layout.addWidget(self.z_spinbox)
        spinbox_layout.addWidget(self.w_spinbox)

        main_layout.addLayout(spinbox_layout)
        self.setLayout(main_layout)

    def _on_value_changed(self):
        self.value_changed.emit(
            (
                self.x_spinbox.value(),
                self.y_spinbox.value(),
                self.z_spinbox.value(),
                self.w_spinbox.value(),
            )
        )

    def set_value(self, value):
        self.x_spinbox.blockSignals(True)
        self.y_spinbox.blockSignals(True)
        self.z_spinbox.blockSignals(True)
        self.w_spinbox.blockSignals(True)
        if hasattr(value, "__iter__") and len(value) >= 4:
            self.x_spinbox.setValue(float(value[0]))
            self.y_spinbox.setValue(float(value[1]))
            self.z_spinbox.setValue(float(value[2]))
            self.w_spinbox.setValue(float(value[3]))
        self.x_spinbox.blockSignals(False)
        self.y_spinbox.blockSignals(False)
        self.z_spinbox.blockSignals(False)
        self.w_spinbox.blockSignals(False)

    def get_value(self):
        return (
            self.x_spinbox.value(),
            self.y_spinbox.value(),
            self.z_spinbox.value(),
            self.w_spinbox.value(),
        )

    def reset_to_default(self):
        """Override to emit the correct tuple format"""
        if self.default_value is not None:
            self.set_value(self.default_value)
            self.value_changed.emit(self.get_value())


# Array-shaped widget values are wrapped in this sentinel before being emitted so
# the generic component/archetype save paths (which coerce bare 2/3/4-length lists
# into vector dicts) pass them through untouched. Dict-shaped values do not need
# wrapping because those paths already forward dicts verbatim.
COLLECTION_SENTINEL_KEY = "__lupine_array__"


class QuatPropertyWidget(PropertyWidget):
    """Widget for editing quaternion properties stored as {w, x, y, z}."""

    def setup_ui(self):
        main_layout = QVBoxLayout()
        main_layout.setContentsMargins(0, 3, 0, 3)
        main_layout.setSpacing(2)

        top_layout = QHBoxLayout()
        top_layout.setSpacing(2)
        self.label = self._make_label()
        top_layout.addWidget(self.label)
        top_layout.addStretch()
        top_layout.addWidget(self._create_reset_button())
        main_layout.addLayout(top_layout)

        spinbox_layout = QHBoxLayout()
        spinbox_layout.setSpacing(2)
        self._spinboxes = {}
        for key in ("w", "x", "y", "z"):
            spin = QDoubleSpinBox()
            spin.setPrefix(f"{key.upper()}:")
            spin.setRange(-1e10, 1e10)
            spin.setDecimals(4)
            spin.setMinimumWidth(40)
            spin.setMinimumHeight(28)
            spin.valueChanged.connect(self._on_value_changed)
            spin.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)
            self._spinboxes[key] = spin
            spinbox_layout.addWidget(spin)
        main_layout.addLayout(spinbox_layout)
        self.setLayout(main_layout)

    def _on_value_changed(self):
        self.value_changed.emit(self.get_value())

    def set_value(self, value):
        if isinstance(value, dict):
            comps = {k: float(value.get(k, 1.0 if k == "w" else 0.0)) for k in ("w", "x", "y", "z")}
        elif hasattr(value, "__iter__") and not isinstance(value, str) and len(list(value)) >= 4:
            seq = list(value)
            comps = {"w": float(seq[0]), "x": float(seq[1]), "y": float(seq[2]), "z": float(seq[3])}
        else:
            comps = {"w": 1.0, "x": 0.0, "y": 0.0, "z": 0.0}
        for key, spin in self._spinboxes.items():
            spin.blockSignals(True)
            spin.setValue(comps[key])
            spin.blockSignals(False)

    def get_value(self):
        return {key: spin.value() for key, spin in self._spinboxes.items()}

    def reset_to_default(self):
        if self.default_value is not None:
            self.set_value(self.default_value)
            self.value_changed.emit(self.get_value())


class RectPropertyWidget(PropertyWidget):
    """Widget for editing rectangle properties stored as {x, y, w, h}."""

    def setup_ui(self):
        main_layout = QVBoxLayout()
        main_layout.setContentsMargins(0, 3, 0, 3)
        main_layout.setSpacing(2)

        top_layout = QHBoxLayout()
        top_layout.setSpacing(2)
        self.label = self._make_label()
        top_layout.addWidget(self.label)
        top_layout.addStretch()
        top_layout.addWidget(self._create_reset_button())
        main_layout.addLayout(top_layout)

        spinbox_layout = QHBoxLayout()
        spinbox_layout.setSpacing(2)
        self._spinboxes = {}
        for key, prefix in (("x", "X:"), ("y", "Y:"), ("w", "W:"), ("h", "H:")):
            spin = QDoubleSpinBox()
            spin.setPrefix(prefix)
            spin.setRange(-1e10, 1e10)
            spin.setDecimals(3)
            spin.setMinimumWidth(40)
            spin.setMinimumHeight(28)
            spin.valueChanged.connect(self._on_value_changed)
            spin.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)
            self._spinboxes[key] = spin
            spinbox_layout.addWidget(spin)
        main_layout.addLayout(spinbox_layout)
        self.setLayout(main_layout)

    def _on_value_changed(self):
        self.value_changed.emit(self.get_value())

    def set_value(self, value):
        if isinstance(value, dict):
            comps = {k: float(value.get(k, 0.0)) for k in ("x", "y", "w", "h")}
        elif hasattr(value, "__iter__") and not isinstance(value, str) and len(list(value)) >= 4:
            seq = list(value)
            comps = {"x": float(seq[0]), "y": float(seq[1]), "w": float(seq[2]), "h": float(seq[3])}
        else:
            comps = {"x": 0.0, "y": 0.0, "w": 0.0, "h": 0.0}
        for key, spin in self._spinboxes.items():
            spin.blockSignals(True)
            spin.setValue(comps[key])
            spin.blockSignals(False)

    def get_value(self):
        return {key: spin.value() for key, spin in self._spinboxes.items()}

    def reset_to_default(self):
        if self.default_value is not None:
            self.set_value(self.default_value)
            self.value_changed.emit(self.get_value())


class _CommitPlainTextEdit(QPlainTextEdit):
    """A plain-text editor that signals when it loses focus, so an owning widget
    can commit/validate its contents only once editing finishes."""
    editing_finished = pyqtSignal()

    def focusOutEvent(self, event):
        super().focusOutEvent(event)
        self.editing_finished.emit()


class CollectionPropertyWidget(PropertyWidget):
    """Widget for editing list/dictionary properties.

    Supported kinds:
      - 'string_array' / 'int_array' / 'float_array': one element per line.
      - 'array': a generic JSON array (heterogeneous values), edited as JSON text.
      - 'dict': a generic JSON object, edited as JSON text.

    Array-shaped kinds emit their value wrapped in COLLECTION_SENTINEL_KEY so the
    save paths do not mistake them for vectors; 'dict' emits a plain dict.
    """

    ARRAY_KINDS = ("string_array", "int_array", "float_array", "array")

    def __init__(self, property_name, kind="array", default_value=None, parent=None):
        self.kind = kind
        super().__init__(property_name, default_value, parent)

    def setup_ui(self):
        main_layout = QVBoxLayout()
        main_layout.setContentsMargins(0, 3, 0, 3)
        main_layout.setSpacing(2)

        top_layout = QHBoxLayout()
        top_layout.setSpacing(2)
        self.label = QLabel(self.property_name)
        self.label.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)
        top_layout.addWidget(self.label)
        top_layout.addWidget(self._create_reset_button())
        main_layout.addLayout(top_layout)

        self.text_edit = _CommitPlainTextEdit()
        self.text_edit.setMinimumHeight(64)
        self.text_edit.setMaximumHeight(160)
        if self.kind in ("array", "dict"):
            placeholder = "[ ... ]" if self.kind == "array" else "{ ... }"
            self.text_edit.setPlaceholderText(f"JSON {placeholder}")
        else:
            self.text_edit.setPlaceholderText("One value per line")
        self.text_edit.editing_finished.connect(self._commit)
        main_layout.addWidget(self.text_edit)

        self._normal_style = self.text_edit.styleSheet()
        self.setLayout(main_layout)

    def _parse(self):
        """Parse the editor contents into a Python value, or raise ValueError."""
        text = self.text_edit.toPlainText()
        if self.kind == "string_array":
            return [ln.strip() for ln in text.splitlines() if ln.strip() != ""]
        if self.kind == "int_array":
            result = []
            for ln in text.splitlines():
                ln = ln.strip()
                if ln == "":
                    continue
                result.append(int(ln))
            return result
        if self.kind == "float_array":
            result = []
            for ln in text.splitlines():
                ln = ln.strip()
                if ln == "":
                    continue
                result.append(float(ln))
            return result
        if self.kind == "array":
            if text.strip() == "":
                return []
            parsed = json.loads(text)
            if not isinstance(parsed, list):
                raise ValueError("Value must be a JSON array")
            return parsed
        if self.kind == "dict":
            if text.strip() == "":
                return {}
            parsed = json.loads(text)
            if not isinstance(parsed, dict):
                raise ValueError("Value must be a JSON object")
            return parsed
        return []

    def _commit(self):
        try:
            parsed = self._parse()
        except (ValueError, TypeError):
            self.text_edit.setStyleSheet("QPlainTextEdit { border: 1px solid #d9534f; }")
            self.text_edit.setToolTip("Invalid value — change not saved")
            return
        self.text_edit.setStyleSheet(self._normal_style)
        self.text_edit.setToolTip("")
        if self.kind == "dict":
            self.value_changed.emit(parsed)
        else:
            self.value_changed.emit({COLLECTION_SENTINEL_KEY: parsed})

    def set_value(self, value):
        self.text_edit.blockSignals(True)
        if self.kind in ("string_array", "int_array", "float_array"):
            items = value if isinstance(value, (list, tuple)) else []
            self.text_edit.setPlainText("\n".join(str(v) for v in items))
        elif self.kind == "array":
            items = value if isinstance(value, list) else []
            self.text_edit.setPlainText(json.dumps(items, indent=2))
        else:  # dict
            obj = value if isinstance(value, dict) else {}
            self.text_edit.setPlainText(json.dumps(obj, indent=2))
        self.text_edit.blockSignals(False)
        self.text_edit.setStyleSheet(self._normal_style)
        self.text_edit.setToolTip("")

    def get_value(self):
        try:
            return self._parse()
        except (ValueError, TypeError):
            return [] if self.kind in self.ARRAY_KINDS else {}

    def reset_to_default(self):
        if self.default_value is not None:
            self.set_value(self.default_value)
            self._commit()


class LinkedVec4PropertyWidget(PropertyWidget):
    """Widget for editing 4D vector properties with linked/unlinked control."""

    def __init__(self, property_name, linked_prop_name=None, default_value=None, labels=None, parent=None):
        self.linked_prop_name = linked_prop_name or f"{property_name}Linked"
        self.is_linked = True
        self._updating_from_backend = False  # Flag to prevent toggle conflicts
        self.labels = labels or ['X', 'Y', 'Z', 'W']  # Default to X, Y, Z, W
        super().__init__(property_name, default_value, parent)

    def setup_ui(self):
        layout = QVBoxLayout()
        layout.setContentsMargins(0, 3, 0, 3)
        layout.setSpacing(2)

        # First row: property label and reset button
        label_layout = QHBoxLayout()
        label_layout.setSpacing(4)

        self.label = self._make_label()
        label_layout.addWidget(self.label)
        label_layout.addStretch()
        label_layout.addWidget(self._create_reset_button())

        layout.addLayout(label_layout)

        # Second row: Edit Linked toggle
        linked_layout = QHBoxLayout()
        linked_layout.setSpacing(4)
        linked_layout.setContentsMargins(0, 0, 0, 0)

        linked_label = QLabel("Edit Linked")
        linked_label.setFixedWidth(120)
        linked_label.setSizePolicy(QSizePolicy.Policy.Fixed, QSizePolicy.Policy.Fixed)
        linked_layout.addWidget(linked_label)

        # Link/unlink toggle button
        self.link_button = QPushButton("")
        self.link_button.setFixedSize(28, 28)
        self.link_button.setCheckable(False)
        self.link_button.setToolTip("Click to toggle linked editing")
        self.link_button.clicked.connect(self._on_link_clicked)
        self._update_button_style()
        linked_layout.addWidget(self.link_button)

        linked_layout.addStretch()

        layout.addLayout(linked_layout)

        # Third row: spinboxes
        spinbox_layout = QHBoxLayout()
        spinbox_layout.setSpacing(2)
        spinbox_layout.setContentsMargins(0, 0, 0, 0)

        self.x_spinbox = QDoubleSpinBox()
        self.x_spinbox.setPrefix(f"{self.labels[0]}:")
        self.x_spinbox.setRange(0, 1e10)
        self.x_spinbox.setDecimals(2)
        self.x_spinbox.setSingleStep(0.1)
        self.x_spinbox.setMinimumWidth(40)
        self.x_spinbox.setMinimumHeight(28)
        self.x_spinbox.valueChanged.connect(lambda: self._on_individual_value_changed(0))
        self.x_spinbox.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)

        self.y_spinbox = QDoubleSpinBox()
        self.y_spinbox.setPrefix(f"{self.labels[1]}:")
        self.y_spinbox.setRange(0, 1e10)
        self.y_spinbox.setDecimals(2)
        self.y_spinbox.setSingleStep(0.1)
        self.y_spinbox.setMinimumWidth(40)
        self.y_spinbox.setMinimumHeight(28)
        self.y_spinbox.valueChanged.connect(lambda: self._on_individual_value_changed(1))
        self.y_spinbox.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)

        self.z_spinbox = QDoubleSpinBox()
        self.z_spinbox.setPrefix(f"{self.labels[2]}:")
        self.z_spinbox.setRange(0, 1e10)
        self.z_spinbox.setDecimals(2)
        self.z_spinbox.setSingleStep(0.1)
        self.z_spinbox.setMinimumWidth(40)
        self.z_spinbox.setMinimumHeight(28)
        self.z_spinbox.valueChanged.connect(lambda: self._on_individual_value_changed(2))
        self.z_spinbox.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)

        self.w_spinbox = QDoubleSpinBox()
        self.w_spinbox.setPrefix(f"{self.labels[3]}:")
        self.w_spinbox.setRange(0, 1e10)
        self.w_spinbox.setDecimals(2)
        self.w_spinbox.setSingleStep(0.1)
        self.w_spinbox.setMinimumWidth(40)
        self.w_spinbox.setMinimumHeight(28)
        self.w_spinbox.valueChanged.connect(lambda: self._on_individual_value_changed(3))
        self.w_spinbox.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)

        spinbox_layout.addWidget(self.x_spinbox)
        spinbox_layout.addWidget(self.y_spinbox)
        spinbox_layout.addWidget(self.z_spinbox)
        spinbox_layout.addWidget(self.w_spinbox)

        layout.addLayout(spinbox_layout)
        self.setLayout(layout)

    def _on_link_clicked(self):
        # Toggle the linked state
        self.is_linked = not self.is_linked
        
        # Update button appearance
        self._update_button_style()
        
        if self.is_linked:
            # When linking, sync all values to X
            value = self.x_spinbox.value()
            self._set_all_spinboxes(value)
        
        # Emit value change to update backend
        self._emit_value_change()

    def _update_button_style(self):
        """Update button style based on linked state"""
        theme = get_theme_manager().get_current_theme()
        if theme:
            colors = theme.colors
            if self.is_linked:
                # Linked state: filled with accent color (like checkbox checked)
                self.link_button.setStyleSheet(f"""
                    QPushButton {{
                        background-color: {colors.accent_color};
                        color: {colors.text_on_accent};
                        border: 2px solid {colors.accent_color};
                        border-radius: 3px;
                    }}
                    QPushButton:hover {{
                        background-color: {colors.accent_hover};
                        border-color: {colors.accent_hover};
                    }}
                """)
            else:
                # Unlinked state: empty with border (like checkbox unchecked)
                self.link_button.setStyleSheet(f"""
                    QPushButton {{
                        background-color: {colors.surface};
                        color: {colors.text_primary};
                        border: 2px solid {colors.border};
                        border-radius: 3px;
                    }}
                    QPushButton:hover {{
                        background-color: {colors.surface_hover};
                        border-color: {colors.border_focus};
                    }}
                """)
        else:
            self.link_button.setStyleSheet("")


    def _on_individual_value_changed(self, index):
        if self.is_linked:
            # Get the value of the changed spinbox
            spinboxes = [self.x_spinbox, self.y_spinbox, self.z_spinbox, self.w_spinbox]
            value = spinboxes[index].value()
            # Update all other spinboxes
            self._set_all_spinboxes(value)
        
        self._emit_value_change()

    def _set_all_spinboxes(self, value):
        """Set all spinboxes to the same value without triggering signals."""
        self.x_spinbox.blockSignals(True)
        self.y_spinbox.blockSignals(True)
        self.z_spinbox.blockSignals(True)
        self.w_spinbox.blockSignals(True)
        
        self.x_spinbox.setValue(value)
        self.y_spinbox.setValue(value)
        self.z_spinbox.setValue(value)
        self.w_spinbox.setValue(value)
        
        self.x_spinbox.blockSignals(False)
        self.y_spinbox.blockSignals(False)
        self.z_spinbox.blockSignals(False)
        self.w_spinbox.blockSignals(False)

    def _emit_value_change(self):
        """Emit value change with both Vec4 and linked state."""
        value_dict = {
            'vec4': (
                self.x_spinbox.value(),
                self.y_spinbox.value(),
                self.z_spinbox.value(),
                self.w_spinbox.value(),
            ),
            'linked': self.is_linked
        }
        self.value_changed.emit(value_dict)

    def set_value(self, value):
        """Set value from Vec4 tuple/list."""
        self.x_spinbox.blockSignals(True)
        self.y_spinbox.blockSignals(True)
        self.z_spinbox.blockSignals(True)
        self.w_spinbox.blockSignals(True)
        
        if hasattr(value, "__iter__") and len(value) >= 4:
            self.x_spinbox.setValue(float(value[0]))
            self.y_spinbox.setValue(float(value[1]))
            self.z_spinbox.setValue(float(value[2]))
            self.w_spinbox.setValue(float(value[3]))
        
        self.x_spinbox.blockSignals(False)
        self.y_spinbox.blockSignals(False)
        self.z_spinbox.blockSignals(False)
        self.w_spinbox.blockSignals(False)

    def set_linked(self, linked):
        """Set the linked state."""
        self.is_linked = linked
        self._update_button_style()

    def get_value(self):
        return {
            'vec4': (
                self.x_spinbox.value(),
                self.y_spinbox.value(),
                self.z_spinbox.value(),
                self.w_spinbox.value(),
            ),
            'linked': self.is_linked
        }

    def reset_to_default(self):
        """Override to emit the correct dict format"""
        if self.default_value is not None:
            self.set_value(self.default_value)
            self.value_changed.emit(self.get_value())



class LinkedColorPropertyWidget(PropertyWidget):
    """Widget for editing 4 color properties with linked/unlinked control (for borders, etc)."""

    def __init__(self, property_name, color_names=None, linked_prop_name=None, default_value=None, parent=None):
        """
        Args:
            property_name: Base name for the property (e.g., "borderColor")
            color_names: List of 4 names for each color (e.g., ["Left", "Right", "Top", "Bottom"])
            linked_prop_name: Name of the linked boolean property
            default_value: Default color tuple (r, g, b, a) to use for all 4 colors
        """
        self.color_names = color_names or ["Color 1", "Color 2", "Color 3", "Color 4"]
        self.linked_prop_name = linked_prop_name or f"{property_name}Linked"
        self.individual_prop_names = [f"{property_name}{name}" for name in self.color_names]
        self.is_linked = True
        self._updating_from_backend = False
        super().__init__(property_name, default_value, parent)

    def setup_ui(self):
        layout = QVBoxLayout()
        layout.setContentsMargins(0, 3, 0, 3)
        layout.setSpacing(2)

        # First row: property label and reset button
        label_layout = QHBoxLayout()
        label_layout.setSpacing(4)

        self.label = self._make_label()
        label_layout.addWidget(self.label)
        label_layout.addStretch()
        label_layout.addWidget(self._create_reset_button())

        layout.addLayout(label_layout)

        # Second row: Edit Linked toggle
        linked_layout = QHBoxLayout()
        linked_layout.setSpacing(4)
        linked_layout.setContentsMargins(0, 0, 0, 0)

        linked_label = QLabel("Edit Linked")
        linked_label.setFixedWidth(120)
        linked_label.setSizePolicy(QSizePolicy.Policy.Fixed, QSizePolicy.Policy.Fixed)
        linked_layout.addWidget(linked_label)

        # Link/unlink toggle button
        self.link_button = QPushButton("")
        self.link_button.setFixedSize(28, 28)
        self.link_button.setCheckable(False)
        self.link_button.setToolTip("Click to toggle linked editing")
        self.link_button.clicked.connect(self._on_link_clicked)
        self._update_button_style()
        linked_layout.addWidget(self.link_button)

        linked_layout.addStretch()

        layout.addLayout(linked_layout)

        # Third row: color buttons in a 2x2 grid
        grid_layout = QVBoxLayout()
        grid_layout.setSpacing(4)
        grid_layout.setContentsMargins(0, 0, 0, 0)

        self.color_buttons = []
        self.current_colors = [QColor(0, 0, 0, 255) for _ in range(4)]

        # Top row: Color 1 and Color 2
        top_row = QHBoxLayout()
        top_row.setSpacing(4)
        for i in range(2):
            color_widget = QWidget()
            color_layout = QVBoxLayout()
            color_layout.setContentsMargins(0, 0, 0, 0)
            color_layout.setSpacing(2)

            color_label = QLabel(self.color_names[i])
            color_label.setAlignment(Qt.AlignmentFlag.AlignCenter)

            color_button = QPushButton()
            color_button.setFixedSize(60, 40)
            color_button.clicked.connect(lambda checked, idx=i: self._choose_color(idx))
            self.color_buttons.append(color_button)

            color_layout.addWidget(color_label)
            color_layout.addWidget(color_button)
            color_widget.setLayout(color_layout)
            top_row.addWidget(color_widget)

        grid_layout.addLayout(top_row)

        # Bottom row: Color 3 and Color 4
        bottom_row = QHBoxLayout()
        bottom_row.setSpacing(4)
        for i in range(2, 4):
            color_widget = QWidget()
            color_layout = QVBoxLayout()
            color_layout.setContentsMargins(0, 0, 0, 0)
            color_layout.setSpacing(2)

            color_label = QLabel(self.color_names[i])
            color_label.setAlignment(Qt.AlignmentFlag.AlignCenter)

            color_button = QPushButton()
            color_button.setFixedSize(60, 40)
            color_button.clicked.connect(lambda checked, idx=i: self._choose_color(idx))
            self.color_buttons.append(color_button)

            color_layout.addWidget(color_label)
            color_layout.addWidget(color_button)
            color_widget.setLayout(color_layout)
            bottom_row.addWidget(color_widget)

        grid_layout.addLayout(bottom_row)

        layout.addLayout(grid_layout)
        self.setLayout(layout)

        # Update button colors initially
        for i in range(4):
            self._update_button_color(i)

    def _on_link_clicked(self):
        # Toggle the linked state
        self.is_linked = not self.is_linked
        
        # Update button appearance
        self._update_button_style()
        
        if self.is_linked:
            # When linking, sync all colors to the first color
            first_color = self.current_colors[0]
            for i in range(1, 4):
                self.current_colors[i] = QColor(first_color)
                self._update_button_color(i)
        
        # Emit value change to update backend
        self._emit_value_change()

    def _update_button_style(self):
        """Update button style based on linked state"""
        theme = get_theme_manager().get_current_theme()
        if theme:
            colors = theme.colors
            if self.is_linked:
                self.link_button.setStyleSheet(f"""
                    QPushButton {{
                        background-color: {colors.accent_color};
                        color: {colors.text_on_accent};
                        border: 2px solid {colors.accent_color};
                        border-radius: 3px;
                    }}
                    QPushButton:hover {{
                        background-color: {colors.accent_hover};
                        border-color: {colors.accent_hover};
                    }}
                """)
            else:
                self.link_button.setStyleSheet(f"""
                    QPushButton {{
                        background-color: {colors.surface};
                        color: {colors.text_primary};
                        border: 2px solid {colors.border};
                        border-radius: 3px;
                    }}
                    QPushButton:hover {{
                        background-color: {colors.surface_hover};
                        border-color: {colors.border_focus};
                    }}
                """)
        else:
            self.link_button.setStyleSheet("")

    def _choose_color(self, index):
        """Open color dialog for a specific color"""
        color = QColorDialog.getColor(self.current_colors[index], self)
        if color.isValid():
            self.current_colors[index] = color
            self._update_button_color(index)

            if self.is_linked:
                # If linked, update all colors to match
                for i in range(4):
                    if i != index:
                        self.current_colors[i] = QColor(color)
                        self._update_button_color(i)

            self._emit_value_change()

    def _update_button_color(self, index):
        """Update the visual appearance of a color button"""
        t = inspector_tokens()
        color = self.current_colors[index]
        self.color_buttons[index].setStyleSheet(
            f"QPushButton {{ background-color: rgb({color.red()}, {color.green()}, {color.blue()});"
            f" border: 1px solid {t.border}; border-radius: 4px; }}"
            f"QPushButton:hover {{ border: 2px solid {t.accent}; }}"
        )

    def _emit_value_change(self):
        """Emit value change with all 4 colors and linked state."""
        value_dict = {
            'colors': [
                (c.redF(), c.greenF(), c.blueF(), c.alphaF())
                for c in self.current_colors
            ],
            'linked': self.is_linked,
            'individual_names': self.individual_prop_names
        }
        self.value_changed.emit(value_dict)

    def set_value(self, colors_list):
        """Set values from a list of 4 color tuples or dicts."""
        if not hasattr(colors_list, "__iter__") or len(colors_list) < 4:
            return

        for i in range(4):
            color = colors_list[i]
            
            # Handle dict format (from C++ backend with r, g, b, a keys)
            if isinstance(color, dict):
                r = int(color.get('r', 0) * 255) if color.get('r', 0) <= 1.0 else int(color.get('r', 0))
                g = int(color.get('g', 0) * 255) if color.get('g', 0) <= 1.0 else int(color.get('g', 0))
                b = int(color.get('b', 0) * 255) if color.get('b', 0) <= 1.0 else int(color.get('b', 0))
                a = int(color.get('a', 1) * 255) if color.get('a', 1) <= 1.0 else int(color.get('a', 1))
                self.current_colors[i] = QColor(r, g, b, a)
                self._update_button_color(i)
            # Handle tuple/list format
            elif hasattr(color, '__iter__') and len(color) >= 3:
                r = int(color[0] * 255) if color[0] <= 1.0 else int(color[0])
                g = int(color[1] * 255) if color[1] <= 1.0 else int(color[1])
                b = int(color[2] * 255) if color[2] <= 1.0 else int(color[2])
                a = int(color[3] * 255) if len(color) > 3 and color[3] <= 1.0 else 255
                self.current_colors[i] = QColor(r, g, b, a)
                self._update_button_color(i)

    def set_linked(self, linked):
        """Set the linked state."""
        self.is_linked = linked
        self._update_button_style()

    def get_value(self):
        return {
            'colors': [
                (c.redF(), c.greenF(), c.blueF(), c.alphaF())
                for c in self.current_colors
            ],
            'linked': self.is_linked,
            'individual_names': self.individual_prop_names
        }

    def reset_to_default(self):
        """Override to emit the correct dict format"""
        if self.default_value is not None:
            # Assume default_value is a single color tuple, apply to all 4
            self.set_value([self.default_value] * 4)
            self.value_changed.emit(self.get_value())



class ColorPropertyWidget(PropertyWidget):
    """Widget for editing color properties"""

    def setup_ui(self):
        layout = QHBoxLayout()
        layout.setContentsMargins(0, 3, 0, 3)
        layout.setSpacing(4)

        self.label = self._make_label()

        self.color_button = QPushButton()
        self.color_button.setFixedSize(64, 26)
        self.color_button.setCursor(Qt.CursorShape.PointingHandCursor)
        self.color_button.setToolTip("Pick color")
        self.color_button.clicked.connect(self._choose_color)
        self.color_button.setSizePolicy(QSizePolicy.Policy.Fixed, QSizePolicy.Policy.Fixed)

        self.current_color = QColor(255, 255, 255)
        self._update_button_color()

        layout.addWidget(self.label)
        layout.addWidget(self.color_button)
        layout.addStretch()
        layout.addWidget(self._create_reset_button())
        self.setLayout(layout)

    def _choose_color(self):
        color = QColorDialog.getColor(self.current_color, self,
                                      options=QColorDialog.ColorDialogOption.ShowAlphaChannel)
        if color.isValid():
            self.current_color = color
            self._update_button_color()
            self.value_changed.emit((color.redF(), color.greenF(), color.blueF(), color.alphaF()))

    def _update_button_color(self):
        t = inspector_tokens()
        c = self.current_color
        self.color_button.setStyleSheet(
            f"QPushButton {{ background-color: rgb({c.red()}, {c.green()}, {c.blue()});"
            f" border: 1px solid {t.border}; border-radius: 4px; }}"
            f"QPushButton:hover {{ border: 2px solid {t.accent}; }}"
        )

    def set_value(self, value):
        if hasattr(value, '__iter__') and len(value) >= 3:
            r = int(value[0] * 255) if value[0] <= 1.0 else int(value[0])
            g = int(value[1] * 255) if value[1] <= 1.0 else int(value[1])
            b = int(value[2] * 255) if value[2] <= 1.0 else int(value[2])
            a = int(value[3] * 255) if len(value) > 3 and value[3] <= 1.0 else 255
            self.current_color = QColor(r, g, b, a)
            self._update_button_color()

    def get_value(self):
        return (self.current_color.redF(), self.current_color.greenF(),
                self.current_color.blueF(), self.current_color.alphaF())

    def reset_to_default(self):
        """Override to emit the correct color format"""
        if self.default_value is not None:
            self.set_value(self.default_value)
            # Emit as tuple of floats (0-1 range)
            self.value_changed.emit(self.get_value())


class AssetDropMixin:
    """Mixin giving a QWidget Unity-style drag-and-drop of project assets.

    A widget mixing this in calls ``self._enable_asset_drops()`` once its controls
    exist, optionally provides ``self._drop_extensions`` (an iterable of accepted
    file extensions; empty/None accepts anything) and implements
    ``self._on_assets_dropped(res_paths)`` to consume the dropped ``res://`` paths.
    The hovered drop target is outlined in the accent colour while a compatible drag
    is over it.
    """

    def _enable_asset_drops(self, highlight_target=None):
        self.setAcceptDrops(True)
        self._drop_highlight_target = highlight_target if highlight_target is not None else self
        self._drop_saved_style = None

    def _drop_accepts(self, mime):
        if not asset_drag.mime_has_assets(mime):
            return False
        extensions = getattr(self, "_drop_extensions", None)
        if not extensions:
            return True
        paths = asset_drag.asset_paths_from_mime(mime)
        return any(asset_drag.matches_extensions(p, extensions) for p in paths)

    def _set_drop_highlight(self, on):
        target = getattr(self, "_drop_highlight_target", None)
        if target is None:
            return
        if on:
            if self._drop_saved_style is None:
                self._drop_saved_style = target.styleSheet()
            t = inspector_tokens()
            target.setStyleSheet(
                (self._drop_saved_style or "")
                + f"\n#assetDropTarget, QLineEdit {{ border: 1px solid {t.accent}; }}")
        else:
            if self._drop_saved_style is not None:
                target.setStyleSheet(self._drop_saved_style)
                self._drop_saved_style = None

    def dragEnterEvent(self, event):
        if self._drop_accepts(event.mimeData()):
            event.acceptProposedAction()
            self._set_drop_highlight(True)
        else:
            event.ignore()

    def dragMoveEvent(self, event):
        if self._drop_accepts(event.mimeData()):
            event.acceptProposedAction()
        else:
            event.ignore()

    def dragLeaveEvent(self, event):
        self._set_drop_highlight(False)
        super().dragLeaveEvent(event)

    def dropEvent(self, event):
        self._set_drop_highlight(False)
        paths = asset_drag.asset_paths_from_mime(event.mimeData())
        extensions = getattr(self, "_drop_extensions", None)
        if extensions:
            paths = [p for p in paths if asset_drag.matches_extensions(p, extensions)]
        if paths:
            event.acceptProposedAction()
            self._on_assets_dropped(paths)
        else:
            event.ignore()

    def _on_assets_dropped(self, res_paths):
        """Consume dropped res:// paths - override in the mixing class."""
        pass


class PathPropertyWidget(AssetDropMixin, PropertyWidget):
    """Widget for editing file path properties (drag-and-drop aware)"""

    def setup_ui(self):
        layout = QHBoxLayout()
        layout.setContentsMargins(0, 3, 0, 3)
        layout.setSpacing(4)

        self.label = self._make_label()

        self.line_edit = QLineEdit()
        self.line_edit.setMinimumWidth(60)
        self.line_edit.setMinimumHeight(26)
        self.line_edit.editingFinished.connect(self._on_editing_finished)
        self.line_edit.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)

        t = inspector_tokens()
        self.browse_button = QPushButton()
        self.browse_button.setIcon(inspector_icon("folder", t.text_secondary))
        self.browse_button.setIconSize(QSize(15, 15))
        self.browse_button.setFixedSize(30, 26)
        self.browse_button.setCursor(Qt.CursorShape.PointingHandCursor)
        self.browse_button.setToolTip("Browse for file")
        self.browse_button.setStyleSheet(f"""
            QPushButton {{
                background-color: {t.surface};
                border: 1px solid {t.border};
                border-radius: 3px;
            }}
            QPushButton:hover {{ border-color: {t.accent}; background-color: {t.surface_hover}; }}
            QPushButton:pressed {{ background-color: {t.accent_muted}; }}
        """)
        self.browse_button.clicked.connect(self._browse_path)

        self.edit_button = QPushButton()
        self.edit_button.setIcon(inspector_icon("edit", t.text_secondary))
        self.edit_button.setIconSize(QSize(15, 15))
        self.edit_button.setFixedSize(30, 26)
        self.edit_button.setCursor(Qt.CursorShape.PointingHandCursor)
        self.edit_button.setToolTip("Open script in the script editor")
        self.edit_button.setStyleSheet(self.browse_button.styleSheet())
        self.edit_button.clicked.connect(self._edit_script)
        self.edit_button.setVisible(False)

        layout.addWidget(self.label)
        layout.addWidget(self.line_edit)
        layout.addWidget(self.edit_button)
        layout.addWidget(self.browse_button)
        layout.addWidget(self._create_reset_button())
        self.setLayout(layout)
        self._enable_asset_drops(self.line_edit)

    def _on_assets_dropped(self, res_paths):
        self.line_edit.setText(res_paths[0])
        self._update_edit_button()
        self.value_changed.emit(res_paths[0])

    def _on_editing_finished(self):
        self._update_edit_button()
        self.value_changed.emit(self.line_edit.text())

    def _is_script_path(self, path: str) -> bool:
        return bool(path) and path.lower().endswith(SCRIPT_FILE_EXTENSIONS)

    def _update_edit_button(self):
        self.edit_button.setVisible(self._is_script_path(self.line_edit.text()))

    def _edit_script(self):
        res_path = self.line_edit.text().strip()
        if not self._is_script_path(res_path):
            return
        abs_path = convert_from_res_path(res_path)

        # Walk up the parent chain to find the main editor window
        main_editor = None
        widget = self.parent()
        while widget is not None:
            if hasattr(widget, '_open_script_in_editor'):
                main_editor = widget
                break
            widget = widget.parent()

        if main_editor is not None:
            main_editor._open_script_in_editor(abs_path)

    def _browse_path(self):
        # Start in project directory if available
        start_dir = get_project_root() or ""
        file_path, _ = QFileDialog.getOpenFileName(self, f"Select {self.property_name}", start_dir)
        if file_path:
            # Convert to res:// path
            res_path = convert_to_res_path(file_path)
            self.line_edit.setText(res_path)
            self._update_edit_button()
            self.value_changed.emit(res_path)

    def set_value(self, value):
        self.line_edit.blockSignals(True)
        self.line_edit.setText(str(value) if value is not None else "")
        self.line_edit.blockSignals(False)
        self._update_edit_button()

    def get_value(self):
        return self.line_edit.text()

    def reset_to_default(self):
        """Override to emit the correct string format"""
        if self.default_value is not None:
            self.set_value(self.default_value)
            self.value_changed.emit(self.get_value())


class TileMapPropertyWidget(PropertyWidget):
    """Widget for editing tilemap file properties with 'Edit' and 'Create' buttons"""

    open_in_editor_requested = pyqtSignal(str)  # Signal to open tilemap in editor

    def __init__(self, property_name, main_editor=None, parent=None):
        self.main_editor = main_editor
        super().__init__(property_name, parent)

    def setup_ui(self):
        main_layout = QVBoxLayout()
        main_layout.setContentsMargins(0, 3, 0, 3)
        main_layout.setSpacing(4)

        # Top row: label, path, browse, reset
        path_layout = QHBoxLayout()
        path_layout.setContentsMargins(0, 0, 0, 0)
        path_layout.setSpacing(4)

        self.label = self._make_label()

        self.line_edit = QLineEdit()
        self.line_edit.setMinimumWidth(60)
        self.line_edit.setMinimumHeight(28)
        self.line_edit.editingFinished.connect(lambda: self.value_changed.emit(self.line_edit.text()))
        self.line_edit.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)

        self.browse_button = QPushButton("...")
        self.browse_button.setFixedSize(30, 28)
        self.browse_button.setToolTip("Browse for tilemap file")
        self.browse_button.clicked.connect(self._browse_path)

        path_layout.addWidget(self.label)
        path_layout.addWidget(self.line_edit)
        path_layout.addWidget(self.browse_button)
        path_layout.addWidget(self._create_reset_button())

        # Bottom row: Edit and Create buttons (aligned with the path field)
        button_layout = QHBoxLayout()
        button_layout.setContentsMargins(0, 0, 0, 0)
        button_layout.setSpacing(4)

        # Spacer to align buttons with the path field (matches label width)
        spacer = QWidget()
        spacer.setFixedWidth(120)

        self.edit_button = QPushButton("Edit")
        self.edit_button.setFixedHeight(28)
        self.edit_button.setToolTip("Open tilemap in TileMap 2D Editor")
        self.edit_button.clicked.connect(self._open_in_editor)

        self.create_button = QPushButton("Create")
        self.create_button.setFixedHeight(28)
        self.create_button.setToolTip("Create a new tilemap in TileMap 2D Editor")
        self.create_button.clicked.connect(self._create_new_tilemap)

        button_layout.addWidget(spacer)
        button_layout.addWidget(self.edit_button)
        button_layout.addWidget(self.create_button)
        button_layout.addStretch()

        main_layout.addLayout(path_layout)
        main_layout.addLayout(button_layout)
        self.setLayout(main_layout)

    def _browse_path(self):
        # Start in project directory if available
        start_dir = get_project_root() or ""
        file_path, _ = QFileDialog.getOpenFileName(
            self,
            "Select TileMap File",
            start_dir,
            "TileMap Files (*.tilemap)"
        )
        if file_path:
            # Convert to res:// path
            res_path = convert_to_res_path(file_path)
            self.line_edit.setText(res_path)
            self.value_changed.emit(res_path)

    def _open_in_editor(self):
        """Open the tilemap file in the TileMap 2D Editor"""
        tilemap_path = self.line_edit.text()

        # Try to find the main editor window
        if self.main_editor:
            # Show the tilemap editor
            if hasattr(self.main_editor, '_show_tilemap_editor'):
                self.main_editor._show_tilemap_editor()

            # Load the tilemap file in the editor
            if 'tilemap_editor' in self.main_editor.panels:
                tilemap_editor = self.main_editor.panels['tilemap_editor']
                if tilemap_path and tilemap_path.strip():
                    tilemap_editor._do_load(tilemap_path)

        # Also emit signal in case other components want to handle it
        self.open_in_editor_requested.emit(tilemap_path)

    def _create_new_tilemap(self):
        """Open the TileMap 2D Editor with a blank canvas"""
        if self.main_editor:
            # Show the tilemap editor
            if hasattr(self.main_editor, '_show_tilemap_editor'):
                self.main_editor._show_tilemap_editor()

            # Clear/reset the tilemap editor to start fresh
            if 'tilemap_editor' in self.main_editor.panels:
                tilemap_editor = self.main_editor.panels['tilemap_editor']

                # Connect to tilemap_saved signal to auto-populate path when saved
                try:
                    # Disconnect any previous connections from this widget
                    tilemap_editor.tilemap_saved.disconnect(self._on_tilemap_saved)
                except (TypeError, RuntimeError):
                    pass  # No previous connection
                tilemap_editor.tilemap_saved.connect(self._on_tilemap_saved)

                if hasattr(tilemap_editor, '_do_new'):
                    tilemap_editor._do_new()
                elif hasattr(tilemap_editor, 'new_tilemap'):
                    tilemap_editor.new_tilemap()

    def _on_tilemap_saved(self, file_path):
        """Called when a tilemap is saved in the editor - auto-populate the path"""
        if file_path:
            self.line_edit.setText(file_path)
            self.value_changed.emit(file_path)

            # Disconnect the signal after use to avoid multiple updates
            if self.main_editor and 'tilemap_editor' in self.main_editor.panels:
                try:
                    self.main_editor.panels['tilemap_editor'].tilemap_saved.disconnect(self._on_tilemap_saved)
                except (TypeError, RuntimeError):
                    pass

    def set_value(self, value):
        self.line_edit.blockSignals(True)
        self.line_edit.setText(str(value) if value is not None else "")
        self.line_edit.blockSignals(False)

    def get_value(self):
        return self.line_edit.text()

    def reset_to_default(self):
        """Override to emit the correct string format"""
        if self.default_value is not None:
            self.set_value(self.default_value)
            self.value_changed.emit(self.get_value())


class AudioFilePropertyWidget(AssetDropMixin, PropertyWidget):
    """Widget for editing audio file properties with playback controls (drag-and-drop aware)"""

    def __init__(self, property_name, editor_bridge=None, parent=None):
        self.editor_bridge = editor_bridge
        self.playing_source_uuid = None
        self._drop_extensions = asset_drag.AUDIO_EXTENSIONS
        super().__init__(property_name, parent)

    def setup_ui(self):
        layout = QHBoxLayout()
        layout.setContentsMargins(0, 3, 0, 3)
        layout.setSpacing(4)

        self.label = self._make_label()

        self.line_edit = QLineEdit()
        self.line_edit.setMinimumWidth(60)
        self.line_edit.setMinimumHeight(28)
        self.line_edit.editingFinished.connect(lambda: self.value_changed.emit(self.line_edit.text()))
        self.line_edit.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)

        self.browse_button = QPushButton("...")
        self.browse_button.setFixedSize(30, 28)
        self.browse_button.clicked.connect(self._browse_path)

        # Audio playback controls
        self.play_button = QPushButton("▶")
        self.play_button.setFixedSize(28, 28)
        self.play_button.setToolTip("Play audio")
        self.play_button.clicked.connect(self._play_audio)

        self.pause_button = QPushButton("⏸")
        self.pause_button.setFixedSize(28, 28)
        self.pause_button.setToolTip("Pause audio")
        self.pause_button.clicked.connect(self._pause_audio)
        self.pause_button.setEnabled(False)

        self.stop_button = QPushButton("⏹")
        self.stop_button.setFixedSize(28, 28)
        self.stop_button.setToolTip("Stop audio")
        self.stop_button.clicked.connect(self._stop_audio)
        self.stop_button.setEnabled(False)

        layout.addWidget(self.label)
        layout.addWidget(self.line_edit)
        layout.addWidget(self.browse_button)
        layout.addWidget(self.play_button)
        layout.addWidget(self.pause_button)
        layout.addWidget(self.stop_button)
        layout.addWidget(self._create_reset_button())
        self.setLayout(layout)
        self._enable_asset_drops(self.line_edit)

    def _on_assets_dropped(self, res_paths):
        self.line_edit.setText(res_paths[0])
        self.value_changed.emit(res_paths[0])

    def _browse_path(self):
        # Start in project directory if available
        start_dir = get_project_root() or ""
        file_path, _ = QFileDialog.getOpenFileName(
            self,
            f"Select {self.property_name}",
            start_dir,
            "Audio Files (*.wav *.mp3 *.ogg *.flac);;All Files (*.*)"
        )
        if file_path:
            # Convert to res:// path
            res_path = convert_to_res_path(file_path)
            self.line_edit.setText(res_path)
            self.value_changed.emit(res_path)

    def _play_audio(self):
        audio_path = self.line_edit.text()
        if not audio_path:
            return

        # Stop any currently playing audio
        self._stop_audio()

        # Play the audio file through editor bridge
        if self.editor_bridge:
            try:
                self.playing_source_uuid = self.editor_bridge.play_audio_file(
                    audio_path,
                    "Master",  # Use Master bus for preview
                    False,     # Don't loop
                    1.0        # Full volume
                )
                # Update button states
                self.play_button.setEnabled(False)
                self.pause_button.setEnabled(True)
                self.stop_button.setEnabled(True)
            except Exception as e:
                print(f"Error playing audio: {e}")

    def _pause_audio(self):
        # Note: miniaudio doesn't have true pause, so we'll just stop for now
        self._stop_audio()

    def _stop_audio(self):
        if self.playing_source_uuid and self.editor_bridge:
            try:
                self.editor_bridge.stop_audio(self.playing_source_uuid)
            except Exception as e:
                print(f"Error stopping audio: {e}")
            finally:
                self.playing_source_uuid = None
                # Update button states
                self.play_button.setEnabled(True)
                self.pause_button.setEnabled(False)
                self.stop_button.setEnabled(False)

    def set_value(self, value):
        self.line_edit.blockSignals(True)
        self.line_edit.setText(str(value) if value is not None else "")
        self.line_edit.blockSignals(False)

    def get_value(self):
        return self.line_edit.text()

    def reset_to_default(self):
        """Override to emit the correct string format"""
        # Stop any playing audio when resetting
        self._stop_audio()
        if self.default_value is not None:
            self.set_value(self.default_value)
            self.value_changed.emit(self.get_value())


def _resource_drop_extensions(extensions, archetype_class, audio_controls):
    """Resolve the accepted extension set for a resource field.

    Explicit extensions win; otherwise derive from the archetype/audio mode; an empty
    tuple means "accept any asset".
    """
    if extensions:
        return tuple(extensions)
    if archetype_class:
        return asset_drag.ARCHETYPE_EXTENSIONS
    if audio_controls:
        return asset_drag.AUDIO_EXTENSIONS
    return ()


class ResourceFieldWidget(AssetDropMixin, PropertyWidget):
    """Unity-style single asset reference field.

    Shows the referenced asset as a drop slot (type icon + name), with a browse and a
    clear button, an optional collapsible image preview and optional audio playback.
    Accepts assets dragged from the file browser or dropped from the OS. The stored
    value is the asset's ``res://`` path (a plain string), so it round-trips through the
    same component/archetype save paths as any other string property.
    """

    def __init__(self, property_name, default_value="", editor_bridge=None,
                 extensions=None, archetype_class="", show_preview=True,
                 audio_controls=False, compact=False, parent=None):
        self.editor_bridge = editor_bridge
        self._archetype_class = archetype_class or ""
        self._show_preview = show_preview
        self._audio_controls = audio_controls
        self._compact = compact
        self._value = ""
        self._preview_expanded = bool(show_preview)
        self.playing_source_uuid = None
        self._drop_extensions = _resource_drop_extensions(
            extensions, self._archetype_class, audio_controls)
        super().__init__(
            property_name, default_value if default_value is not None else "", parent)

    def setup_ui(self):
        t = inspector_tokens()
        outer = QVBoxLayout()
        outer.setContentsMargins(0, 3, 0, 3)
        outer.setSpacing(4)

        row = QHBoxLayout()
        row.setContentsMargins(0, 0, 0, 0)
        row.setSpacing(4)

        if not self._compact:
            self.label = self._make_label()
            row.addWidget(self.label)

        self.slot = QFrame()
        self.slot.setObjectName("assetDropTarget")
        self.slot.setMinimumHeight(26)
        self.slot.setStyleSheet(
            f"QFrame#assetDropTarget {{ background-color: {t.surface};"
            f" border: 1px solid {t.border}; border-radius: 3px; }}")
        slot_layout = QHBoxLayout(self.slot)
        slot_layout.setContentsMargins(6, 2, 6, 2)
        slot_layout.setSpacing(6)
        self.slot_icon = QLabel()
        self.slot_icon.setFixedSize(16, 16)
        self.slot_name = QLabel("None")
        self.slot_name.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Preferred)
        slot_layout.addWidget(self.slot_icon)
        slot_layout.addWidget(self.slot_name, 1)
        row.addWidget(self.slot, 1)

        button_qss = (
            f"QPushButton {{ background-color: {t.surface}; border: 1px solid {t.border};"
            f" border-radius: 3px; }}"
            f"QPushButton:hover {{ border-color: {t.accent}; background-color: {t.surface_hover}; }}"
            f"QPushButton:pressed {{ background-color: {t.accent_muted}; }}"
            f"QPushButton:disabled {{ color: {t.text_disabled}; border-color: {with_alpha(t.border, 0.5)}; }}")

        self.preview_btn = QPushButton()
        self.preview_btn.setIcon(inspector_icon("image", t.text_secondary))
        self.preview_btn.setIconSize(QSize(15, 15))
        self.preview_btn.setFixedSize(28, 26)
        self.preview_btn.setCheckable(True)
        self.preview_btn.setCursor(Qt.CursorShape.PointingHandCursor)
        self.preview_btn.setToolTip("Toggle image preview")
        self.preview_btn.setStyleSheet(button_qss)
        self.preview_btn.clicked.connect(self._toggle_preview)
        self.preview_btn.setVisible(False)
        row.addWidget(self.preview_btn)

        if self._audio_controls:
            self.play_button = QPushButton("▶")
            self.play_button.setFixedSize(28, 26)
            self.play_button.setToolTip("Play audio")
            self.play_button.setStyleSheet(button_qss)
            self.play_button.clicked.connect(self._play_audio)
            row.addWidget(self.play_button)
            self.stop_button = QPushButton("⏹")
            self.stop_button.setFixedSize(28, 26)
            self.stop_button.setToolTip("Stop audio")
            self.stop_button.setStyleSheet(button_qss)
            self.stop_button.clicked.connect(self._stop_audio)
            self.stop_button.setEnabled(False)
            row.addWidget(self.stop_button)

        self.browse_button = QPushButton()
        self.browse_button.setIcon(inspector_icon("folder", t.text_secondary))
        self.browse_button.setIconSize(QSize(15, 15))
        self.browse_button.setFixedSize(28, 26)
        self.browse_button.setCursor(Qt.CursorShape.PointingHandCursor)
        self.browse_button.setToolTip("Browse for asset")
        self.browse_button.setStyleSheet(button_qss)
        self.browse_button.clicked.connect(self._browse)
        row.addWidget(self.browse_button)

        self.clear_button = QPushButton()
        self.clear_button.setIcon(inspector_icon("close", t.text_secondary))
        self.clear_button.setIconSize(QSize(15, 15))
        self.clear_button.setFixedSize(28, 26)
        self.clear_button.setCursor(Qt.CursorShape.PointingHandCursor)
        self.clear_button.setToolTip("Clear reference")
        self.clear_button.setStyleSheet(button_qss)
        self.clear_button.clicked.connect(self._clear)
        row.addWidget(self.clear_button)

        if not self._compact:
            row.addWidget(self._create_reset_button())

        outer.addLayout(row)

        self.preview_label = QLabel()
        self.preview_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.preview_label.setStyleSheet(
            f"background-color: {t.surface}; border: 1px solid {with_alpha(t.border, 0.6)};"
            f" border-radius: 3px; padding: 4px;")
        self.preview_label.setVisible(False)
        outer.addWidget(self.preview_label)

        self.setLayout(outer)
        self._enable_asset_drops(self.slot)
        self._refresh_display()

    def _file_dialog_filter(self):
        if self._archetype_class:
            return "Archetype Instances (*.ares);;All Files (*.*)"
        if self._drop_extensions:
            patterns = " ".join("*" + e for e in self._drop_extensions)
            return f"Assets ({patterns});;All Files (*.*)"
        return "All Files (*.*)"

    def _browse(self):
        start_dir = get_project_root() or ""
        file_path, _ = QFileDialog.getOpenFileName(
            self, f"Select {self.property_name}", start_dir, self._file_dialog_filter())
        if file_path:
            self._set_path(convert_to_res_path(file_path))

    def _clear(self):
        self._set_path("")

    def _on_assets_dropped(self, res_paths):
        self._set_path(res_paths[0])

    def _set_path(self, path, emit=True):
        self._value = str(path) if path else ""
        self._refresh_display()
        if emit:
            self.value_changed.emit(self._value)

    def _refresh_display(self):
        t = inspector_tokens()
        path = self._value
        if path:
            name = path.rstrip("/").rsplit("/", 1)[-1]
            self.slot_name.setText(name)
            self.slot_name.setStyleSheet(
                f"color: {t.text_primary}; background: transparent;")
            self.slot.setToolTip(path)
            self.clear_button.setEnabled(True)
        else:
            self.slot_name.setText("None  (drag an asset here)")
            self.slot_name.setStyleSheet(
                f"color: {t.text_disabled}; font-style: italic; background: transparent;")
            self.slot.setToolTip("")
            self.clear_button.setEnabled(False)
        self._update_type_icon(path)
        self._update_preview(path)
        if self._audio_controls:
            self.play_button.setEnabled(bool(path) and asset_drag.is_audio_path(path))

    def _update_type_icon(self, path):
        t = inspector_tokens()
        if asset_drag.is_image_path(path):
            name = "image"
        elif asset_drag.is_archetype_path(path):
            name = "duplicate"
        else:
            name = "folder"
        self.slot_icon.setPixmap(inspector_icon(name, t.text_secondary).pixmap(QSize(15, 15)))

    def _update_preview(self, path):
        previewable = self._show_preview and asset_drag.is_previewable_image(path)
        if not previewable:
            self.preview_btn.setVisible(False)
            self.preview_label.setVisible(False)
            self.preview_label.clear()
            return
        pixmap = QPixmap(asset_drag.from_res_path(path))
        if pixmap.isNull():
            self.preview_btn.setVisible(False)
            self.preview_label.setVisible(False)
            self.preview_label.clear()
            return
        scaled = pixmap.scaled(
            240, 160, Qt.AspectRatioMode.KeepAspectRatio,
            Qt.TransformationMode.SmoothTransformation)
        self.preview_label.setPixmap(scaled)
        self.preview_label.setToolTip(f"{pixmap.width()} x {pixmap.height()} px")
        self.preview_btn.setVisible(True)
        self.preview_btn.setChecked(self._preview_expanded)
        self.preview_label.setVisible(self._preview_expanded)

    def _toggle_preview(self):
        self._preview_expanded = not self._preview_expanded
        self.preview_btn.setChecked(self._preview_expanded)
        has_pixmap = (self.preview_label.pixmap() is not None
                      and not self.preview_label.pixmap().isNull())
        self.preview_label.setVisible(self._preview_expanded and has_pixmap)

    def _play_audio(self):
        if not self._value or not self.editor_bridge:
            return
        self._stop_audio()
        try:
            self.playing_source_uuid = self.editor_bridge.play_audio_file(
                self._value, "Master", False, 1.0)
            self.play_button.setEnabled(False)
            self.stop_button.setEnabled(True)
        except Exception as e:
            print(f"Error playing audio: {e}")

    def _stop_audio(self):
        if self.playing_source_uuid and self.editor_bridge:
            try:
                self.editor_bridge.stop_audio(self.playing_source_uuid)
            except Exception as e:
                print(f"Error stopping audio: {e}")
            finally:
                self.playing_source_uuid = None
        if self._audio_controls:
            self.play_button.setEnabled(bool(self._value) and asset_drag.is_audio_path(self._value))
            self.stop_button.setEnabled(False)

    def set_value(self, value):
        self._value = str(value) if value else ""
        self._refresh_display()

    def get_value(self):
        return self._value

    def reset_to_default(self):
        self._set_path(self.default_value if self.default_value else "")


class ResourceArrayFieldWidget(AssetDropMixin, PropertyWidget):
    """Unity-style multi-asset reference field.

    A collapsible list of asset references (each a :class:`ResourceFieldWidget`), with
    add / remove / reorder controls. Assets dragged onto the list are appended (a
    multi-file drop appends them all). The value is emitted wrapped in
    ``COLLECTION_SENTINEL_KEY`` so the array of ``res://`` paths is stored verbatim and
    not coerced into a vector by the generic save path.
    """

    def __init__(self, property_name, default_value=None, editor_bridge=None,
                 extensions=None, archetype_class="", show_preview=True,
                 audio_controls=False, parent=None):
        self.editor_bridge = editor_bridge
        self._archetype_class = archetype_class or ""
        self._show_preview = show_preview
        self._audio_controls = audio_controls
        self._explicit_extensions = tuple(extensions) if extensions else None
        self._drop_extensions = _resource_drop_extensions(
            extensions, self._archetype_class, audio_controls)
        self._paths = []
        self._expanded = True
        super().__init__(
            property_name, default_value if default_value is not None else [], parent)

    def setup_ui(self):
        t = inspector_tokens()
        outer = QVBoxLayout()
        outer.setContentsMargins(0, 3, 0, 3)
        outer.setSpacing(2)

        header = QHBoxLayout()
        header.setContentsMargins(0, 0, 0, 0)
        header.setSpacing(4)

        self.foldout_btn = QPushButton()
        self.foldout_btn.setFixedSize(16, 16)
        self.foldout_btn.setFlat(True)
        self.foldout_btn.setCursor(Qt.CursorShape.PointingHandCursor)
        self.foldout_btn.setStyleSheet("QPushButton { border: none; background: transparent; }")
        self.foldout_btn.clicked.connect(self._toggle_expanded)
        header.addWidget(self.foldout_btn)

        self.label = self._make_label()
        header.addWidget(self.label)

        self.count_label = QLabel("(0)")
        self.count_label.setStyleSheet(
            f"color: {t.text_disabled}; background: transparent;")
        header.addWidget(self.count_label)
        header.addStretch()

        button_qss = (
            f"QPushButton {{ background-color: {t.surface}; border: 1px solid {t.border};"
            f" border-radius: 3px; }}"
            f"QPushButton:hover {{ border-color: {t.accent}; background-color: {t.surface_hover}; }}"
            f"QPushButton:pressed {{ background-color: {t.accent_muted}; }}"
            f"QPushButton:disabled {{ color: {t.text_disabled}; border-color: {with_alpha(t.border, 0.5)}; }}")
        self._button_qss = button_qss

        self.add_button = QPushButton()
        self.add_button.setIcon(inspector_icon("plus", t.text_secondary))
        self.add_button.setIconSize(QSize(15, 15))
        self.add_button.setFixedSize(28, 24)
        self.add_button.setCursor(Qt.CursorShape.PointingHandCursor)
        self.add_button.setToolTip("Add asset(s)")
        self.add_button.setStyleSheet(button_qss)
        self.add_button.clicked.connect(self._add_via_dialog)
        header.addWidget(self.add_button)
        header.addWidget(self._create_reset_button())

        outer.addLayout(header)

        self.body = QFrame()
        self.body.setObjectName("assetDropTarget")
        self.body.setStyleSheet(
            f"QFrame#assetDropTarget {{ background-color: {with_alpha(t.surface, 0.5)};"
            f" border: 1px dashed {with_alpha(t.border, 0.8)}; border-radius: 4px; }}")
        self.body_layout = QVBoxLayout(self.body)
        self.body_layout.setContentsMargins(8, 6, 6, 6)
        self.body_layout.setSpacing(4)
        outer.addWidget(self.body)

        self.setLayout(outer)
        self._enable_asset_drops(self.body)
        self._rebuild()

    def _toggle_expanded(self):
        self._expanded = not self._expanded
        self.body.setVisible(self._expanded)
        self._update_foldout_icon()

    def _update_foldout_icon(self):
        t = inspector_tokens()
        self.foldout_btn.setText("▾" if self._expanded else "▸")
        self.foldout_btn.setStyleSheet(
            f"QPushButton {{ border: none; background: transparent;"
            f" color: {t.text_secondary}; font-size: 10px; }}")

    def _element_extensions(self):
        return self._explicit_extensions

    def _clear_body(self):
        while self.body_layout.count():
            item = self.body_layout.takeAt(0)
            widget = item.widget()
            if widget is not None:
                widget.setParent(None)
                widget.deleteLater()

    def _rebuild(self):
        t = inspector_tokens()
        self._clear_body()
        if not self._paths:
            empty = QLabel("Drag assets here, or press +")
            empty.setStyleSheet(
                f"color: {t.text_disabled}; font-style: italic; background: transparent;")
            self.body_layout.addWidget(empty)
        else:
            for index, path in enumerate(self._paths):
                self.body_layout.addWidget(self._make_element_row(index, path))
        self.count_label.setText(f"({len(self._paths)})")
        self._update_foldout_icon()
        self.body.setVisible(self._expanded)

    def _make_element_row(self, index, path):
        t = inspector_tokens()
        container = QFrame()
        container.setStyleSheet("background: transparent;")
        v = QVBoxLayout(container)
        v.setContentsMargins(0, 0, 0, 0)
        v.setSpacing(0)

        row = QHBoxLayout()
        row.setContentsMargins(0, 0, 0, 0)
        row.setSpacing(4)

        idx_label = QLabel(str(index))
        idx_label.setFixedWidth(20)
        idx_label.setAlignment(Qt.AlignmentFlag.AlignRight | Qt.AlignmentFlag.AlignVCenter)
        idx_label.setStyleSheet(f"color: {t.text_disabled}; background: transparent;")
        row.addWidget(idx_label)

        field = ResourceFieldWidget(
            "", path, editor_bridge=self.editor_bridge,
            extensions=self._element_extensions(), archetype_class=self._archetype_class,
            show_preview=self._show_preview, audio_controls=self._audio_controls,
            compact=True)
        field.value_changed.connect(lambda value, i=index: self._on_element_changed(i, value))
        row.addWidget(field, 1)

        up_btn = QPushButton("▲")
        up_btn.setFixedSize(24, 24)
        up_btn.setStyleSheet(self._button_qss)
        up_btn.setToolTip("Move up")
        up_btn.setEnabled(index > 0)
        up_btn.clicked.connect(lambda _=False, i=index: self._move_element(i, -1))
        row.addWidget(up_btn)

        down_btn = QPushButton("▼")
        down_btn.setFixedSize(24, 24)
        down_btn.setStyleSheet(self._button_qss)
        down_btn.setToolTip("Move down")
        down_btn.setEnabled(index < len(self._paths) - 1)
        down_btn.clicked.connect(lambda _=False, i=index: self._move_element(i, 1))
        row.addWidget(down_btn)

        remove_btn = QPushButton()
        remove_btn.setIcon(inspector_icon("close", t.text_secondary))
        remove_btn.setIconSize(QSize(14, 14))
        remove_btn.setFixedSize(24, 24)
        remove_btn.setStyleSheet(self._button_qss)
        remove_btn.setToolTip("Remove")
        remove_btn.clicked.connect(lambda _=False, i=index: self._remove_element(i))
        row.addWidget(remove_btn)

        v.addLayout(row)
        return container

    def _on_element_changed(self, index, value):
        if 0 <= index < len(self._paths):
            self._paths[index] = str(value) if value else ""
            self._emit()

    def _remove_element(self, index):
        if 0 <= index < len(self._paths):
            del self._paths[index]
            self._rebuild()
            self._emit()

    def _move_element(self, index, delta):
        target = index + delta
        if 0 <= index < len(self._paths) and 0 <= target < len(self._paths):
            self._paths[index], self._paths[target] = self._paths[target], self._paths[index]
            self._rebuild()
            self._emit()

    def _add_via_dialog(self):
        start_dir = get_project_root() or ""
        if self._archetype_class:
            filt = "Archetype Instances (*.ares);;All Files (*.*)"
        elif self._explicit_extensions:
            patterns = " ".join("*" + e for e in self._explicit_extensions)
            filt = f"Assets ({patterns});;All Files (*.*)"
        else:
            filt = "All Files (*.*)"
        files, _ = QFileDialog.getOpenFileNames(
            self, f"Add {self.property_name}", start_dir, filt)
        if files:
            for file_path in files:
                self._paths.append(convert_to_res_path(file_path))
            self._expanded = True
            self._rebuild()
            self._emit()

    def _on_assets_dropped(self, res_paths):
        self._paths.extend(res_paths)
        self._expanded = True
        self._rebuild()
        self._emit()

    def _emit(self):
        self.count_label.setText(f"({len(self._paths)})")
        self.value_changed.emit({COLLECTION_SENTINEL_KEY: list(self._paths)})

    def set_value(self, value):
        if isinstance(value, dict) and COLLECTION_SENTINEL_KEY in value:
            value = value[COLLECTION_SENTINEL_KEY]
        if isinstance(value, (list, tuple)):
            self._paths = [str(v) for v in value if v is not None]
        else:
            self._paths = []
        self._rebuild()

    def get_value(self):
        return {COLLECTION_SENTINEL_KEY: list(self._paths)}

    def reset_to_default(self):
        default = self.default_value
        if isinstance(default, dict) and COLLECTION_SENTINEL_KEY in default:
            default = default[COLLECTION_SENTINEL_KEY]
        if isinstance(default, (list, tuple)):
            self._paths = [str(v) for v in default if v is not None]
        else:
            self._paths = []
        self._rebuild()
        self._emit()


class MaterialOverridePropertyWidget(PropertyWidget):
    """Widget for editing material override properties with collapsible categories.

    Supports shader selection (built-in and custom) with dynamic parameter categories
    based on the selected shader type.
    """

    def __init__(self, property_name, component, editor_bridge=None, parent=None):
        self.component = component
        self.editor_bridge = editor_bridge
        self.category_widgets = {}
        self.current_shader_name = "PBR"  # Default shader
        self._custom_shader_path = ""
        super().__init__(property_name, parent)

    def setup_ui(self):
        # Import shader definitions
        from editor.widgets.shader_definitions import get_shader_registry, ParameterType, GraphicsBackend
        self.shader_registry = get_shader_registry()

        # Set backend from editor bridge if available
        self._update_backend_from_engine()

        main_layout = QVBoxLayout()
        main_layout.setContentsMargins(0, 0, 0, 0)
        main_layout.setSpacing(2)

        # Header with enable checkbox
        header_layout = QHBoxLayout()
        header_layout.setContentsMargins(0, 3, 0, 3)
        header_layout.setSpacing(4)

        self.enabled_checkbox = QCheckBox("Material Override")
        self.enabled_checkbox.setChecked(False)
        self.enabled_checkbox.toggled.connect(self._on_enabled_changed)
        header_layout.addWidget(self.enabled_checkbox)

        header_layout.addStretch()

        main_layout.addLayout(header_layout)

        # Container for shader settings and categories (shown when enabled)
        self.categories_widget = QWidget()
        self.categories_layout = QVBoxLayout()
        self.categories_layout.setContentsMargins(10, 0, 0, 0)
        self.categories_layout.setSpacing(2)
        self.categories_widget.setLayout(self.categories_layout)
        self.categories_widget.setVisible(False)

        # Shader selection dropdown
        self._create_shader_selector()

        # Custom shader path (initially hidden)
        self._create_custom_shader_path()

        # Dynamic categories container
        self.dynamic_categories_widget = QWidget()
        self.dynamic_categories_layout = QVBoxLayout()
        self.dynamic_categories_layout.setContentsMargins(0, 0, 0, 0)
        self.dynamic_categories_layout.setSpacing(2)
        self.dynamic_categories_widget.setLayout(self.dynamic_categories_layout)
        self.categories_layout.addWidget(self.dynamic_categories_widget)

        # Build categories for default shader (PBR)
        self._rebuild_categories()

        main_layout.addWidget(self.categories_widget)
        self.setLayout(main_layout)

    def _create_shader_selector(self):
        """Create the shader selection dropdown"""
        from editor.theme import get_theme_manager
        theme = get_theme_manager().get_current_theme()
        colors = theme.colors

        shader_layout = QHBoxLayout()
        shader_layout.setContentsMargins(0, 2, 0, 4)
        shader_layout.setSpacing(4)

        shader_label = QLabel("Shader:")
        shader_label.setStyleSheet(f"color: {colors.text_primary}; font-weight: bold;")
        shader_layout.addWidget(shader_label)

        self.shader_combo = QComboBox()
        # Add built-in shaders
        for shader_name in self.shader_registry.get_builtin_shader_names():
            self.shader_combo.addItem(shader_name)
        # Add "Custom" option
        self.shader_combo.addItem("Custom")

        self.shader_combo.setCurrentText("PBR")
        self.shader_combo.currentTextChanged.connect(self._on_shader_changed)
        self.shader_combo.setStyleSheet(f"""
            QComboBox {{
                background-color: {colors.surface};
                color: {colors.text_primary};
                border: 1px solid {colors.border};
                border-radius: 3px;
                padding: 3px 6px;
            }}
            QComboBox:hover {{
                background-color: {colors.surface_hover};
            }}
            QComboBox::drop-down {{
                border: none;
            }}
            QComboBox::down-arrow {{
                image: none;
                border-left: 4px solid transparent;
                border-right: 4px solid transparent;
                border-top: 6px solid {colors.text_secondary};
            }}
            QComboBox QAbstractItemView {{
                background-color: {colors.surface};
                color: {colors.text_primary};
                selection-background-color: {colors.accent_color};
            }}
        """)
        shader_layout.addWidget(self.shader_combo, 1)

        self.categories_layout.addLayout(shader_layout)

    def _create_custom_shader_path(self):
        """Create the custom shader path browser (hidden by default)"""
        from editor.theme import get_theme_manager
        theme = get_theme_manager().get_current_theme()
        colors = theme.colors

        # Get backend-specific labels
        backend = self.shader_registry.get_current_backend()

        self.custom_shader_widget = QWidget()
        custom_layout = QVBoxLayout()
        custom_layout.setContentsMargins(0, 0, 0, 4)
        custom_layout.setSpacing(2)

        # Vertex shader path
        vert_layout = QHBoxLayout()
        vert_layout.setSpacing(4)
        self.vert_shader_label = QLabel(f"{backend.vertex_shader_name}:")
        self.vert_shader_label.setStyleSheet(f"color: {colors.text_secondary};")
        self.vert_shader_label.setFixedWidth(120)
        vert_layout.addWidget(self.vert_shader_label)

        self.vert_shader_edit = QLineEdit()
        self.vert_shader_edit.setPlaceholderText(backend.vertex_file_placeholder)
        self.vert_shader_edit.setStyleSheet(f"""
            QLineEdit {{
                background-color: {colors.surface};
                color: {colors.text_primary};
                border: 1px solid {colors.border};
                border-radius: 3px;
                padding: 3px;
            }}
        """)
        vert_layout.addWidget(self.vert_shader_edit, 1)

        vert_browse = QPushButton("...")
        vert_browse.setFixedWidth(30)
        vert_browse.clicked.connect(lambda: self._browse_shader("vertex"))
        vert_layout.addWidget(vert_browse)
        custom_layout.addLayout(vert_layout)

        # Fragment shader path
        frag_layout = QHBoxLayout()
        frag_layout.setSpacing(4)
        self.frag_shader_label = QLabel(f"{backend.fragment_shader_name}:")
        self.frag_shader_label.setStyleSheet(f"color: {colors.text_secondary};")
        self.frag_shader_label.setFixedWidth(120)
        frag_layout.addWidget(self.frag_shader_label)

        self.frag_shader_edit = QLineEdit()
        self.frag_shader_edit.setPlaceholderText(backend.fragment_file_placeholder)
        self.frag_shader_edit.setStyleSheet(f"""
            QLineEdit {{
                background-color: {colors.surface};
                color: {colors.text_primary};
                border: 1px solid {colors.border};
                border-radius: 3px;
                padding: 3px;
            }}
        """)
        frag_layout.addWidget(self.frag_shader_edit, 1)

        frag_browse = QPushButton("...")
        frag_browse.setFixedWidth(30)
        frag_browse.clicked.connect(lambda: self._browse_shader("fragment"))
        frag_layout.addWidget(frag_browse)
        custom_layout.addLayout(frag_layout)

        # Apply button for custom shaders
        apply_layout = QHBoxLayout()
        apply_layout.addStretch()
        self.apply_shader_btn = QPushButton("Apply Custom Shader")
        self.apply_shader_btn.clicked.connect(self._apply_custom_shader)
        self.apply_shader_btn.setStyleSheet(f"""
            QPushButton {{
                background-color: {colors.accent_color};
                color: {colors.text_primary};
                border: none;
                border-radius: 3px;
                padding: 4px 12px;
            }}
            QPushButton:hover {{
                background-color: {colors.accent_hover};
            }}
        """)
        apply_layout.addWidget(self.apply_shader_btn)
        custom_layout.addLayout(apply_layout)

        self.custom_shader_widget.setLayout(custom_layout)
        self.custom_shader_widget.setVisible(False)
        self.categories_layout.addWidget(self.custom_shader_widget)

    def _update_backend_from_engine(self):
        """Update the shader registry with the current graphics backend from the engine"""
        if self.editor_bridge:
            try:
                engine_backend = self.editor_bridge.get_backend()
                self.shader_registry.set_current_backend_from_engine(engine_backend)
            except Exception as e:
                print(f"[Warning] Could not get graphics backend: {e}")

    def _browse_shader(self, shader_type):
        """Browse for a shader file"""
        # Get appropriate file filter based on current backend
        backend = self.shader_registry.get_current_backend()
        from editor.widgets.shader_definitions import GraphicsBackend

        if backend in (GraphicsBackend.OPENGL, GraphicsBackend.WEBGL):
            ext_filter = "GLSL Shader (*.vert *.frag *.glsl)"
        elif backend == GraphicsBackend.VULKAN:
            ext_filter = "SPIR-V Shader (*.spv);;GLSL Source (*.vert *.frag *.glsl)"
        elif backend in (GraphicsBackend.DIRECTX11, GraphicsBackend.DIRECTX12):
            ext_filter = "HLSL Shader (*.hlsl)"
        elif backend == GraphicsBackend.METAL:
            ext_filter = "Metal Shader (*.metal)"
        else:
            ext_filter = "Vertex Shader (*.vert *.glsl)" if shader_type == "vertex" else "Fragment Shader (*.frag *.glsl)"

        file_filter = ext_filter
        start_dir = get_project_root() or ""
        file_path, _ = QFileDialog.getOpenFileName(self, f"Select {shader_type.title()} Shader", start_dir, file_filter)
        if file_path:
            # Convert to res:// path
            res_path = convert_to_res_path(file_path)
            if shader_type == "vertex":
                self.vert_shader_edit.setText(res_path)
            else:
                self.frag_shader_edit.setText(res_path)

    def _apply_custom_shader(self):
        """Apply the custom shader and reload parameters"""
        from editor.widgets.shader_definitions import CustomShaderLoader

        vert_path = self.vert_shader_edit.text()
        frag_path = self.frag_shader_edit.text()

        if not vert_path and not frag_path:
            return

        # Try to load shader definition from metadata or parse shader
        shader_def = CustomShaderLoader.load_from_file(vert_path or frag_path)
        if shader_def:
            self.shader_registry.register_custom_shader(shader_def)
            self._custom_shader_path = vert_path or frag_path
            self._rebuild_categories(shader_def)

            # Notify engine of custom shader change
            if self.component and self.editor_bridge:
                try:
                    import json
                    self.editor_bridge.set_component_property(
                        self.component, 'customShaderVert', json.dumps(vert_path))
                    self.editor_bridge.set_component_property(
                        self.component, 'customShaderFrag', json.dumps(frag_path))
                except Exception as e:
                    print(f"[Warning] Failed to set custom shader: {e}")

    def _on_shader_changed(self, shader_name):
        """Handle shader selection change"""
        self.current_shader_name = shader_name

        # Show/hide custom shader path widget
        is_custom = (shader_name == "Custom")
        self.custom_shader_widget.setVisible(is_custom)

        if not is_custom:
            # Rebuild categories for built-in shader
            shader_def = self.shader_registry.get_shader_by_display_name(shader_name)
            if shader_def:
                self._rebuild_categories(shader_def)

                # Notify engine of shader change
                if self.component and self.editor_bridge:
                    try:
                        import json
                        self.editor_bridge.set_component_property(
                            self.component, 'shaderType', json.dumps(shader_def.name))
                    except Exception as e:
                        print(f"[Warning] Failed to set shader type: {e}")

    def _rebuild_categories(self, shader_def=None):
        """Rebuild the material categories based on the current shader"""
        from editor.widgets.shader_definitions import ParameterType

        # Get shader definition if not provided
        if shader_def is None:
            shader_def = self.shader_registry.get_shader_by_display_name(self.current_shader_name)
            if shader_def is None:
                shader_def = self.shader_registry.get_shader("PBR")

        # Clear existing categories
        self._clear_categories()

        # Build new categories from shader definition
        for category in shader_def.categories:
            cat_widgets = self._create_category(category.display_name)

            for param in category.parameters:
                widget = self._create_parameter_widget(param)
                if widget:
                    cat_widgets['layout'].addWidget(widget)
                    cat_widgets['widgets'][param.name] = widget

    def _clear_categories(self):
        """Clear all category widgets"""
        # Remove all widgets from dynamic categories layout
        while self.dynamic_categories_layout.count():
            item = self.dynamic_categories_layout.takeAt(0)
            if item.widget():
                item.widget().deleteLater()
        self.category_widgets.clear()

    def _create_parameter_widget(self, param):
        """Create a widget for a shader parameter based on its type"""
        from editor.widgets.shader_definitions import ParameterType

        if param.param_type == ParameterType.FLOAT:
            widget = FloatPropertyWidget(param.display_name)
            if param.default_value is not None:
                widget.set_value(param.default_value)
            widget.value_changed.connect(lambda v, p=param.name: self._set_parameter(p, v))
            return widget

        elif param.param_type == ParameterType.COLOR:
            widget = ColorPropertyWidget(param.display_name)
            if param.default_value:
                from PyQt6.QtGui import QColor
                widget.set_value(QColor.fromRgbF(*param.default_value[:4]))
            widget.value_changed.connect(lambda v, p=param.name: self._set_color_parameter(p, v))
            return widget

        elif param.param_type == ParameterType.TEXTURE:
            widget = PathPropertyWidget(param.display_name)
            widget.value_changed.connect(lambda v, p=param.name: self._set_texture_parameter(p, v))
            return widget

        elif param.param_type == ParameterType.VEC2:
            widget = Vec2PropertyWidget(param.display_name)
            widget.value_changed.connect(lambda v, p=param.name: self._set_parameter(p, v))
            return widget

        elif param.param_type == ParameterType.VEC3:
            widget = Vec3PropertyWidget(param.display_name)
            widget.value_changed.connect(lambda v, p=param.name: self._set_parameter(p, v))
            return widget

        elif param.param_type == ParameterType.VEC4:
            widget = Vec4PropertyWidget(param.display_name)
            widget.value_changed.connect(lambda v, p=param.name: self._set_parameter(p, v))
            return widget

        elif param.param_type == ParameterType.INT:
            widget = IntPropertyWidget(param.display_name)
            if param.default_value is not None:
                widget.set_value(param.default_value)
            widget.value_changed.connect(lambda v, p=param.name: self._set_parameter(p, v))
            return widget

        elif param.param_type == ParameterType.BOOL:
            widget = BoolPropertyWidget(param.display_name)
            if param.default_value is not None:
                widget.set_value(param.default_value)
            widget.value_changed.connect(lambda v, p=param.name: self._set_parameter(p, v))
            return widget

        return None

    def _set_parameter(self, param_name, value):
        """Set a generic parameter value"""
        if self.component and self.editor_bridge:
            try:
                import json
                self.editor_bridge.set_component_property(self.component, param_name, json.dumps(value))
            except Exception as e:
                print(f"[Warning] Failed to set {param_name}: {e}")

    def _set_color_parameter(self, param_name, color):
        """Set a color parameter"""
        if self.component and self.editor_bridge:
            try:
                import json
                if isinstance(color, QColor):
                    color_dict = {'r': color.redF(), 'g': color.greenF(), 'b': color.blueF(), 'a': color.alphaF()}
                elif isinstance(color, (tuple, list)) and len(color) >= 3:
                    color_dict = {'r': float(color[0]), 'g': float(color[1]), 'b': float(color[2]), 'a': float(color[3]) if len(color) > 3 else 1.0}
                else:
                    return
                self.editor_bridge.set_component_property(self.component, param_name, json.dumps(color_dict))
            except Exception as e:
                print(f"[Warning] Failed to set {param_name}: {e}")

    def _set_texture_parameter(self, param_name, path):
        """Set a texture parameter"""
        # TODO: Load texture and set handle
        if self.component and self.editor_bridge:
            try:
                import json
                self.editor_bridge.set_component_property(self.component, param_name + "Path", json.dumps(path))
            except Exception as e:
                print(f"[Warning] Failed to set {param_name}: {e}")

    def _create_category(self, name):
        """Create a collapsible category group"""
        # Get theme colors
        from editor.theme import get_theme_manager
        theme = get_theme_manager().get_current_theme()
        colors = theme.colors

        # Container for the category
        category_container = QWidget()
        container_layout = QVBoxLayout()
        container_layout.setContentsMargins(0, 0, 0, 0)
        container_layout.setSpacing(0)
        category_container.setLayout(container_layout)

        # Category header button
        header_btn = QPushButton(f"▶ {name}")
        header_btn.setCheckable(True)
        header_btn.setStyleSheet(f"""
            QPushButton {{
                text-align: left;
                padding: 4px;
                border: 1px solid {colors.border};
                background-color: {colors.surface};
                color: {colors.text_primary};
            }}
            QPushButton:checked {{
                background-color: {colors.surface_hover};
            }}
            QPushButton:hover {{
                background-color: {colors.surface_hover};
            }}
        """)
        container_layout.addWidget(header_btn)

        # Category content widget
        content_widget = QWidget()
        content_layout = QVBoxLayout()
        content_layout.setContentsMargins(10, 2, 0, 2)
        content_layout.setSpacing(2)
        content_widget.setLayout(content_layout)
        content_widget.setVisible(False)
        container_layout.addWidget(content_widget)

        # Connect toggle
        def toggle_category():
            is_expanded = header_btn.isChecked()
            content_widget.setVisible(is_expanded)
            header_btn.setText(f"{'▼' if is_expanded else '▶'} {name}")

        header_btn.toggled.connect(toggle_category)

        # Add to dynamic categories layout
        self.dynamic_categories_layout.addWidget(category_container)

        # Store category info
        category = {
            'container': category_container,
            'header': header_btn,
            'content': content_widget,
            'layout': content_layout,
            'widgets': {}
        }
        self.category_widgets[name] = category
        return category

    def _on_enabled_changed(self, enabled):
        """Handle material override enabled/disabled"""
        self.categories_widget.setVisible(enabled)
        if self.component and self.editor_bridge:
            try:
                import json
                self.editor_bridge.set_component_property(self.component, 'materialOverrideEnabled', json.dumps(enabled))
            except Exception as e:
                print(f"[Warning] Failed to set material override enabled: {e}")
        self.value_changed.emit(enabled)

    def set_value(self, value):
        """Set material override values from component"""
        if not self.component or not self.editor_bridge:
            return

        try:
            import json
            props_json = self.editor_bridge.get_component_properties(self.component)
            all_properties = json.loads(props_json) if props_json else {}
            properties = all_properties.get('properties', {})

            # Set enabled state
            has_override = properties.get('materialOverrideEnabled', False)
            self.enabled_checkbox.blockSignals(True)
            self.enabled_checkbox.setChecked(has_override)
            self.enabled_checkbox.blockSignals(False)
            self.categories_widget.setVisible(has_override)

            # Set shader type if stored
            shader_type = properties.get('shaderType', 'PBR')
            if shader_type and shader_type != self.current_shader_name:
                shader_def = self.shader_registry.get_shader(shader_type)
                if shader_def:
                    self.shader_combo.blockSignals(True)
                    self.shader_combo.setCurrentText(shader_def.display_name)
                    self.shader_combo.blockSignals(False)
                    self.current_shader_name = shader_def.display_name
                    self._rebuild_categories(shader_def)

            # Update widget values from properties
            self._load_parameter_values(properties)

        except Exception as e:
            print(f"[Warning] Failed to load material override values: {e}")

    def _load_parameter_values(self, properties):
        """Load parameter values from component properties into widgets"""
        from editor.widgets.shader_definitions import ParameterType

        # Map property names to widget lookups
        # This handles the dynamic categories created from shader definitions
        for category_name, category_data in self.category_widgets.items():
            widgets = category_data.get('widgets', {})
            for param_name, widget in widgets.items():
                if param_name in properties:
                    value = properties[param_name]
                    try:
                        # Handle color values
                        if isinstance(value, dict) and 'r' in value:
                            qcolor = QColor.fromRgbF(
                                value.get('r', 1.0),
                                value.get('g', 1.0),
                                value.get('b', 1.0),
                                value.get('a', 1.0)
                            )
                            widget.set_value(qcolor)
                        else:
                            widget.set_value(value)
                    except Exception as e:
                        print(f"[Warning] Failed to set widget value for {param_name}: {e}")

    def get_value(self):
        """Get current material override state"""
        return self.enabled_checkbox.isChecked()


class MediaPreviewControlWidget(QWidget):
    """Inspector controls for VideoPlayer / GifPlayer.

    Provides a Replay button (restart playback from the beginning, even for a
    finished one-shot) plus Play/Pause, and for videos a togglable seek scrubber.
    These drive the component live in the edit viewport via the editor bridge's
    call_component_method (the editor ticks component OnUpdate continuously while
    editing), so a designer can preview without entering Play mode.
    """

    def __init__(self, component, editor_bridge, show_seek=False, parent=None):
        super().__init__(parent)
        self.component = component
        self.editor_bridge = editor_bridge
        self.show_seek = show_seek
        self._user_scrubbing = False
        self._timer = None
        self._build_ui()
        if self.show_seek:
            self._timer = QTimer(self)
            self._timer.timeout.connect(self._poll_position)
            self._timer.start(200)

    def _build_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 4, 0, 0)
        layout.setSpacing(4)

        button_row = QHBoxLayout()
        self.replay_btn = QPushButton("↻ Replay")
        self.replay_btn.setToolTip("Restart playback from the beginning")
        self.replay_btn.setCursor(Qt.CursorShape.PointingHandCursor)
        self.replay_btn.clicked.connect(lambda: self._call("replay"))
        button_row.addWidget(self.replay_btn)

        self.play_btn = QPushButton("▶ Play")
        self.play_btn.setToolTip("Play")
        self.play_btn.setCursor(Qt.CursorShape.PointingHandCursor)
        self.play_btn.clicked.connect(lambda: self._call("play"))
        button_row.addWidget(self.play_btn)

        self.pause_btn = QPushButton("⏸ Pause")
        self.pause_btn.setToolTip("Pause")
        self.pause_btn.setCursor(Qt.CursorShape.PointingHandCursor)
        self.pause_btn.clicked.connect(lambda: self._call("pause"))
        button_row.addWidget(self.pause_btn)
        layout.addLayout(button_row)

        if self.show_seek:
            seek_row = QHBoxLayout()
            self.slider = QSlider(Qt.Orientation.Horizontal)
            self.slider.setRange(0, 1000)
            self.slider.setToolTip("Seek (requires 'Seek Enabled')")
            self.slider.sliderPressed.connect(self._on_scrub_press)
            self.slider.sliderReleased.connect(self._on_scrub_release)
            seek_row.addWidget(self.slider, 1)

            self.time_label = QLabel("0.0 / 0.0s")
            seek_row.addWidget(self.time_label)
            layout.addLayout(seek_row)

    def _call(self, method, args=None):
        if not (self.component and self.editor_bridge):
            return None
        try:
            return self.editor_bridge.call_component_method(
                self.component, method, json.dumps(args if args is not None else []))
        except Exception as e:
            print(f"[MediaPreviewControl] {method} failed: {e}")
            return None

    def _call_num(self, method):
        res = self._call(method)
        if not res:
            return 0.0
        try:
            return float(json.loads(res))
        except (ValueError, TypeError):
            return 0.0

    def _on_scrub_press(self):
        self._user_scrubbing = True

    def _on_scrub_release(self):
        duration = self._call_num("get_duration")
        if duration > 0.0:
            target = (self.slider.value() / 1000.0) * duration
            self._call("seek", [target])
        self._user_scrubbing = False

    def _poll_position(self):
        if self._user_scrubbing or not self.isVisible():
            return
        duration = self._call_num("get_duration")
        position = self._call_num("get_position")
        if duration > 0.0:
            frac = max(0.0, min(1.0, position / duration))
            self.slider.blockSignals(True)
            self.slider.setValue(int(frac * 1000))
            self.slider.blockSignals(False)
        self.time_label.setText(f"{position:.1f} / {duration:.1f}s")


class ButtonStatePropertyWidget(PropertyWidget):
    """Widget for editing Button state properties with automatic/manual mode toggle"""

    def __init__(self, property_name, component, editor_bridge=None, parent=None):
        self.component = component
        self.editor_bridge = editor_bridge
        self.state_widgets = {}
        self.is_automatic_mode = True
        super().__init__(property_name, parent)

    def setup_ui(self):
        main_layout = QVBoxLayout()
        main_layout.setContentsMargins(0, 0, 0, 0)
        main_layout.setSpacing(2)

        # Header with mode toggle
        header_layout = QHBoxLayout()
        header_layout.setContentsMargins(0, 3, 0, 3)
        header_layout.setSpacing(4)

        header_label = QLabel("Button States")
        header_label.setStyleSheet("font-weight: bold;")
        header_layout.addWidget(header_label)

        header_layout.addStretch()

        # Mode toggle
        self.mode_combo = QComboBox()
        self.mode_combo.addItems(["Automatic", "Manual"])
        self.mode_combo.currentIndexChanged.connect(self._on_mode_changed)
        header_layout.addWidget(QLabel("Mode:"))
        header_layout.addWidget(self.mode_combo)

        main_layout.addLayout(header_layout)

        # Container for state editors
        self.states_widget = QWidget()
        self.states_layout = QVBoxLayout()
        self.states_layout.setContentsMargins(10, 0, 0, 0)
        self.states_layout.setSpacing(2)
        self.states_widget.setLayout(self.states_layout)

        # Create state editors
        self._create_state_editor("Normal", 0)
        self._create_state_editor("Hover", 1)
        self._create_state_editor("Pressed", 2)
        self._create_state_editor("Disabled", 3)

        main_layout.addWidget(self.states_widget)
        self.setLayout(main_layout)

    def _create_state_editor(self, state_name, state_index):
        """Create a collapsible state editor"""
        from editor.theme import get_theme_manager
        theme = get_theme_manager().get_current_theme()
        colors = theme.colors if theme else None

        # State header button
        header_btn = QPushButton(f"▶ {state_name}")
        header_btn.setCheckable(True)
        if colors:
            header_btn.setStyleSheet(f"""
                QPushButton {{
                    text-align: left;
                    padding: 4px;
                    border: 1px solid {colors.border};
                    background-color: {colors.surface};
                    color: {colors.text_primary};
                }}
                QPushButton:checked {{
                    background-color: {colors.surface_hover};
                }}
                QPushButton:hover {{
                    background-color: {colors.surface_hover};
                }}
            """)

        # State content widget
        content_widget = QWidget()
        content_layout = QVBoxLayout()
        content_layout.setContentsMargins(10, 2, 0, 2)
        content_layout.setSpacing(2)
        content_widget.setLayout(content_layout)
        content_widget.setVisible(False)

        # Modulation color (for automatic mode)
        modulation_widget = ColorPropertyWidget(f"{state_name} Modulation")
        modulation_widget.value_changed.connect(
            lambda v: self._set_state_modulation(state_name.lower(), v))
        content_layout.addWidget(modulation_widget)

        # StyleBox (for manual mode) - show background color, border, corner radius
        stylebox_container = QWidget()
        stylebox_layout = QVBoxLayout()
        stylebox_layout.setContentsMargins(0, 0, 0, 0)
        stylebox_layout.setSpacing(2)
        stylebox_container.setLayout(stylebox_layout)
        content_layout.addWidget(stylebox_container)

        # StyleBox background color
        stylebox_bg_widget = ColorPropertyWidget(f"{state_name} BG Color")
        stylebox_bg_widget.value_changed.connect(
            lambda v: self._set_state_stylebox_property(state_name.lower(), 'BackgroundColor', v))
        stylebox_layout.addWidget(stylebox_bg_widget)

        # StyleBox border color
        stylebox_border_widget = ColorPropertyWidget(f"{state_name} Border Color")
        stylebox_border_widget.value_changed.connect(
            lambda v: self._set_state_stylebox_property(state_name.lower(), 'BorderColor', v))
        stylebox_layout.addWidget(stylebox_border_widget)

        # Sound path
        sound_widget = PathPropertyWidget(f"{state_name} Sound")
        sound_widget.value_changed.connect(
            lambda v: self._set_state_sound(state_name.lower(), v))
        content_layout.addWidget(sound_widget)

        # Tween enabled checkbox
        tween_enabled_widget = BoolPropertyWidget(f"{state_name} Tween Enabled")
        tween_enabled_widget.value_changed.connect(
            lambda v: self._on_tween_enabled_changed(state_name, v))
        content_layout.addWidget(tween_enabled_widget)

        # Tween settings container (hidden by default)
        tween_container = QWidget()
        tween_layout = QVBoxLayout()
        tween_layout.setContentsMargins(10, 2, 0, 2)
        tween_layout.setSpacing(2)
        tween_container.setLayout(tween_layout)
        tween_container.setVisible(False)
        content_layout.addWidget(tween_container)

        # Tween scale
        tween_scale_widget = Vec2PropertyWidget(f"{state_name} Tween Scale")
        tween_scale_widget.value_changed.connect(
            lambda v: self._set_state_tween_property(state_name.lower(), 'Scale', v))
        tween_layout.addWidget(tween_scale_widget)

        # Tween rotation
        tween_rotation_widget = FloatPropertyWidget(f"{state_name} Tween Rotation")
        tween_rotation_widget.value_changed.connect(
            lambda v: self._set_state_tween_property(state_name.lower(), 'Rotation', v))
        tween_layout.addWidget(tween_rotation_widget)

        # Tween position
        tween_position_widget = Vec2PropertyWidget(f"{state_name} Tween Position")
        tween_position_widget.value_changed.connect(
            lambda v: self._set_state_tween_property(state_name.lower(), 'Position', v))
        tween_layout.addWidget(tween_position_widget)

        # Tween duration
        tween_duration_widget = FloatPropertyWidget(f"{state_name} Tween Duration")
        tween_duration_widget.value_changed.connect(
            lambda v: self._set_state_tween_property(state_name.lower(), 'Duration', v))
        tween_layout.addWidget(tween_duration_widget)

        # Preview button
        preview_btn = QPushButton(f"Preview {state_name} Tween")
        preview_btn.clicked.connect(lambda: self._preview_tween(state_name))
        tween_layout.addWidget(preview_btn)

        # Connect toggle
        def toggle_state():
            is_expanded = header_btn.isChecked()
            content_widget.setVisible(is_expanded)
            header_btn.setText(f"{'▼' if is_expanded else '▶'} {state_name}")

        header_btn.toggled.connect(toggle_state)

        # Add to states layout
        self.states_layout.addWidget(header_btn)
        self.states_layout.addWidget(content_widget)

        # Store state info
        self.state_widgets[state_name] = {
            'header': header_btn,
            'content': content_widget,
            'layout': content_layout,
            'modulation': modulation_widget,
            'stylebox_container': stylebox_container,
            'stylebox_bg': stylebox_bg_widget,
            'stylebox_border': stylebox_border_widget,
            'sound': sound_widget,
            'tween_enabled': tween_enabled_widget,
            'tween_container': tween_container,
            'tween_scale': tween_scale_widget,
            'tween_rotation': tween_rotation_widget,
            'tween_position': tween_position_widget,
            'tween_duration': tween_duration_widget,
            'preview_btn': preview_btn
        }

    def _on_mode_changed(self, index):
        """Handle mode change between Automatic and Manual"""
        self.is_automatic_mode = (index == 0)

        # Update visibility of mode-specific widgets
        for state_name, widgets in self.state_widgets.items():
            if 'modulation' in widgets:
                widgets['modulation'].setVisible(self.is_automatic_mode)
            if 'stylebox_container' in widgets:
                widgets['stylebox_container'].setVisible(not self.is_automatic_mode)

        # Set property on component
        if self.component and self.editor_bridge:
            try:
                import json
                self.editor_bridge.set_component_property(
                    self.component, 'styleMode', json.dumps(index))
            except Exception as e:
                print(f"[Warning] Failed to set button style mode: {e}")

    def _set_state_modulation(self, state_name, color):
        """Set state modulation color"""
        if self.component and self.editor_bridge:
            try:
                import json
                prop_name = f"{state_name}Modulation"
                if isinstance(color, QColor):
                    color_dict = {'r': color.redF(), 'g': color.greenF(),
                                'b': color.blueF(), 'a': color.alphaF()}
                elif isinstance(color, (tuple, list)) and len(color) >= 3:
                    color_dict = {'r': float(color[0]), 'g': float(color[1]),
                                'b': float(color[2]), 'a': float(color[3]) if len(color) > 3 else 1.0}
                else:
                    return
                self.editor_bridge.set_component_property(
                    self.component, prop_name, json.dumps(color_dict))
            except Exception as e:
                print(f"[Warning] Failed to set {state_name} modulation: {e}")

    def _set_state_sound(self, state_name, path):
        """Set state sound path"""
        if self.component and self.editor_bridge:
            try:
                import json
                prop_name = f"{state_name}SoundPath"
                self.editor_bridge.set_component_property(
                    self.component, prop_name, json.dumps(path))
            except Exception as e:
                print(f"[Warning] Failed to set {state_name} sound: {e}")

    def _set_state_stylebox_property(self, state_name, prop_suffix, value):
        """Set state StyleBox property (BackgroundColor, BorderColor, etc.)"""
        if self.component and self.editor_bridge:
            try:
                import json
                # For manual mode, we need to set properties on the StyleBox
                # For now, we'll just log - full StyleBox implementation needed
                print(f"[Info] Set {state_name} StyleBox {prop_suffix}: {value}")
                # TODO: Implement StyleBox property setting through bridge
            except Exception as e:
                print(f"[Warning] Failed to set {state_name} StyleBox {prop_suffix}: {e}")

    def _set_state_tween_enabled(self, state_name, enabled):
        """Set state tween enabled"""
        if self.component and self.editor_bridge:
            try:
                import json
                prop_name = f"{state_name}TweenEnabled"
                self.editor_bridge.set_component_property(
                    self.component, prop_name, json.dumps(enabled))
            except Exception as e:
                print(f"[Warning] Failed to set {state_name} tween enabled: {e}")

    def _on_tween_enabled_changed(self, state_name, enabled):
        """Handle tween enabled checkbox change"""
        # Update tween container visibility
        widgets = self.state_widgets.get(state_name, {})
        if 'tween_container' in widgets:
            widgets['tween_container'].setVisible(enabled)

        # Set property
        self._set_state_tween_enabled(state_name.lower(), enabled)

    def _set_state_tween_property(self, state_name, prop_suffix, value):
        """Set state tween property (Scale, Rotation, Position, Duration)"""
        if self.component and self.editor_bridge:
            try:
                import json
                prop_name = f"{state_name}Tween{prop_suffix}"

                # Convert value to appropriate format
                if isinstance(value, (tuple, list)) and len(value) >= 2:
                    # Vec2
                    value_dict = {'x': float(value[0]), 'y': float(value[1])}
                    self.editor_bridge.set_component_property(
                        self.component, prop_name, json.dumps(value_dict))
                else:
                    # Float
                    self.editor_bridge.set_component_property(
                        self.component, prop_name, json.dumps(float(value)))
            except Exception as e:
                print(f"[Warning] Failed to set {state_name} tween {prop_suffix}: {e}")

    def _preview_tween(self, state_name):
        """Preview tween for a specific state"""
        if self.component and self.editor_bridge:
            try:
                # Call PreviewTween method on the component
                # Map state name to ButtonState enum value
                state_map = {'Normal': 0, 'Hover': 1, 'Pressed': 2, 'Disabled': 3}
                state_value = state_map.get(state_name, 0)

                # Call the C++ method directly
                # The component has a PreviewTween(ButtonState) method
                self.component.PreviewTween(state_value)
                print(f"[Info] Previewed tween for {state_name} (state={state_value})")
            except Exception as e:
                print(f"[Warning] Failed to preview tween for {state_name}: {e}")

    def set_value(self, value):
        """Set button state values from component"""
        if not self.component or not self.editor_bridge:
            return

        try:
            import json
            props_json = self.editor_bridge.get_component_properties(self.component)
            properties = json.loads(props_json) if props_json else {}
            props = properties.get('properties', {})

            # Set mode
            if 'styleMode' in props:
                self.mode_combo.blockSignals(True)
                self.mode_combo.setCurrentIndex(props['styleMode'])
                self.mode_combo.blockSignals(False)
                self.is_automatic_mode = (props['styleMode'] == 0)

            # Update state widgets
            for state_name in ['Normal', 'Hover', 'Pressed', 'Disabled']:
                state_lower = state_name.lower()
                widgets = self.state_widgets.get(state_name, {})

                # Modulation color
                mod_prop = f"{state_lower}Modulation"
                if mod_prop in props and 'modulation' in widgets:
                    color = props[mod_prop]
                    qcolor = QColor.fromRgbF(color.get('r', 1.0), color.get('g', 1.0),
                                           color.get('b', 1.0), color.get('a', 1.0))
                    widgets['modulation'].set_value(qcolor)

                # Sound path
                sound_prop = f"{state_lower}SoundPath"
                if sound_prop in props and 'sound' in widgets:
                    widgets['sound'].set_value(props[sound_prop])

                # Tween enabled
                tween_enabled_prop = f"{state_lower}TweenEnabled"
                tween_enabled = props.get(tween_enabled_prop, False)
                if 'tween_enabled' in widgets:
                    widgets['tween_enabled'].set_value(tween_enabled)

                # Show/hide tween container based on enabled state
                if 'tween_container' in widgets:
                    widgets['tween_container'].setVisible(tween_enabled)

                # Tween scale
                tween_scale_prop = f"{state_lower}TweenScale"
                if tween_scale_prop in props and 'tween_scale' in widgets:
                    scale = props[tween_scale_prop]
                    widgets['tween_scale'].set_value((scale.get('x', 0.0), scale.get('y', 0.0)))

                # Tween rotation
                tween_rotation_prop = f"{state_lower}TweenRotation"
                if tween_rotation_prop in props and 'tween_rotation' in widgets:
                    widgets['tween_rotation'].set_value(props[tween_rotation_prop])

                # Tween position
                tween_position_prop = f"{state_lower}TweenPosition"
                if tween_position_prop in props and 'tween_position' in widgets:
                    pos = props[tween_position_prop]
                    widgets['tween_position'].set_value((pos.get('x', 0.0), pos.get('y', 0.0)))

                # Tween duration
                tween_duration_prop = f"{state_lower}TweenDuration"
                if tween_duration_prop in props and 'tween_duration' in widgets:
                    widgets['tween_duration'].set_value(props[tween_duration_prop])

            # Update visibility based on mode
            for state_name, widgets in self.state_widgets.items():
                if 'modulation' in widgets:
                    widgets['modulation'].setVisible(self.is_automatic_mode)
                if 'stylebox_container' in widgets:
                    widgets['stylebox_container'].setVisible(not self.is_automatic_mode)

        except Exception as e:
            print(f"[Warning] Failed to load button state properties: {e}")

    def get_value(self):
        """Get current button state mode"""
        return self.mode_combo.currentIndex()


class WorldEnvironmentPropertyWidget(PropertyWidget):
    """Custom widget for WorldEnvironment component with collapsable categories and dynamic fields"""

    def __init__(self, property_name, component, editor_bridge=None, parent=None):
        self.component = component
        self.editor_bridge = editor_bridge
        self.category_widgets = {}
        super().__init__(property_name, parent)

    def setup_ui(self):
        main_layout = QVBoxLayout()
        main_layout.setContentsMargins(0, 0, 0, 0)
        main_layout.setSpacing(2)

        # Create collapsable categories
        self._create_skybox_category(main_layout)
        self._create_fog_category(main_layout)
        self._create_ambient_light_category(main_layout)
        self._create_volumetric_fog_category(main_layout)

        self.setLayout(main_layout)

    def _create_float_widget(self, label_text, default_value, min_val=None, max_val=None, step=None, decimals=3):
        """Create a custom float widget with specified parameters"""
        widget = QWidget()
        layout = QHBoxLayout()
        layout.setContentsMargins(0, 2, 0, 2)
        layout.setSpacing(5)

        label = QLabel(label_text)
        label.setFixedWidth(120)
        layout.addWidget(label)

        spinbox = QDoubleSpinBox()
        spinbox.setRange(min_val if min_val is not None else -1e10,
                        max_val if max_val is not None else 1e10)
        spinbox.setDecimals(decimals)
        spinbox.setSingleStep(step if step is not None else 0.1)
        spinbox.setValue(default_value)
        spinbox.setMinimumWidth(60)
        spinbox.setMinimumHeight(28)
        layout.addWidget(spinbox)

        widget.setLayout(layout)
        widget.spinbox = spinbox  # Store reference for easy access
        return widget

    def _create_category_header(self, title, is_expanded=True):
        """Create a collapsable category header button"""
        from editor.theme import get_theme_manager
        theme = get_theme_manager().get_current_theme()
        colors = theme.colors if theme else None

        header_btn = QPushButton(f"{'▼' if is_expanded else '▶'} {title}")
        header_btn.setCheckable(True)
        header_btn.setChecked(is_expanded)
        if colors:
            header_btn.setStyleSheet(f"""
                QPushButton {{
                    text-align: left;
                    padding: 6px;
                    border: 1px solid {colors.border};
                    background-color: {colors.surface};
                    color: {colors.text_primary};
                    font-weight: bold;
                }}
                QPushButton:checked {{
                    background-color: {colors.surface_hover};
                }}
                QPushButton:hover {{
                    background-color: {colors.surface_hover};
                }}
            """)

        return header_btn

    def _create_skybox_category(self, parent_layout):
        """Create skybox settings category"""
        # Header
        header_btn = self._create_category_header("Skybox", True)
        parent_layout.addWidget(header_btn)

        # Content widget
        content_widget = QWidget()
        content_layout = QVBoxLayout()
        content_layout.setContentsMargins(10, 2, 0, 2)
        content_layout.setSpacing(2)
        content_widget.setLayout(content_layout)

        # Skybox type enum
        skybox_type_widget = EnumPropertyWidget("Skybox Type",
            ["None", "Color", "Procedural", "Cubemap", "Panoramic"], 0)
        skybox_type_widget.value_changed.connect(self._on_skybox_type_changed)
        content_layout.addWidget(skybox_type_widget)

        # Color skybox settings (visible when type = Color)
        color_container = QWidget()
        color_layout = QVBoxLayout()
        color_layout.setContentsMargins(10, 0, 0, 0)
        color_layout.setSpacing(2)
        color_container.setLayout(color_layout)

        skybox_color_widget = ColorPropertyWidget("Skybox Color", (0.5, 0.7, 1.0, 1.0))
        skybox_color_widget.value_changed.connect(lambda v: self._set_property('skyboxColor', v))
        color_layout.addWidget(skybox_color_widget)

        content_layout.addWidget(color_container)
        color_container.setVisible(False)

        # Procedural skybox settings (visible when type = Procedural)
        procedural_container = QWidget()
        procedural_layout = QVBoxLayout()
        procedural_layout.setContentsMargins(10, 0, 0, 0)
        procedural_layout.setSpacing(2)
        procedural_container.setLayout(procedural_layout)

        sky_top_widget = ColorPropertyWidget("Sky Top Color", (0.1, 0.3, 0.8, 1.0))
        sky_top_widget.value_changed.connect(lambda v: self._set_property('skyTopColor', v))
        procedural_layout.addWidget(sky_top_widget)

        sky_horizon_widget = ColorPropertyWidget("Sky Horizon Color", (0.6, 0.7, 0.9, 1.0))
        sky_horizon_widget.value_changed.connect(lambda v: self._set_property('skyHorizonColor', v))
        procedural_layout.addWidget(sky_horizon_widget)

        sky_bottom_widget = ColorPropertyWidget("Sky Bottom Color", (0.8, 0.8, 0.8, 1.0))
        sky_bottom_widget.value_changed.connect(lambda v: self._set_property('skyBottomColor', v))
        procedural_layout.addWidget(sky_bottom_widget)

        content_layout.addWidget(procedural_container)
        procedural_container.setVisible(False)

        # Cubemap skybox settings (visible when type = Cubemap)
        cubemap_container = QWidget()
        cubemap_layout = QVBoxLayout()
        cubemap_layout.setContentsMargins(10, 0, 0, 0)
        cubemap_layout.setSpacing(2)
        cubemap_container.setLayout(cubemap_layout)

        for face_name in ["PosX", "NegX", "PosY", "NegY", "PosZ", "NegZ"]:
            face_widget = PathPropertyWidget(f"Cubemap {face_name}", "")
            face_widget.value_changed.connect(
                lambda v, name=face_name: self._set_property(f'cubemap{name}', v))
            cubemap_layout.addWidget(face_widget)

        content_layout.addWidget(cubemap_container)
        cubemap_container.setVisible(False)

        # Panoramic skybox settings (visible when type = Panoramic)
        panoramic_container = QWidget()
        panoramic_layout = QVBoxLayout()
        panoramic_layout.setContentsMargins(10, 0, 0, 0)
        panoramic_layout.setSpacing(2)
        panoramic_container.setLayout(panoramic_layout)

        panoramic_widget = PathPropertyWidget("Panoramic Texture", "")
        panoramic_widget.value_changed.connect(lambda v: self._set_property('panoramicTexture', v))
        panoramic_layout.addWidget(panoramic_widget)

        content_layout.addWidget(panoramic_container)
        panoramic_container.setVisible(False)

        parent_layout.addWidget(content_widget)

        # Store cubemap widgets for later access
        cubemap_widgets = {}
        for i, face_name in enumerate(["PosX", "NegX", "PosY", "NegY", "PosZ", "NegZ"]):
            cubemap_widgets[face_name.lower()] = cubemap_layout.itemAt(i).widget()

        # Store widgets for later access
        self.category_widgets['skybox'] = {
            'header': header_btn,
            'content': content_widget,
            'type_widget': skybox_type_widget,
            'color_container': color_container,
            'color_widget': skybox_color_widget,
            'procedural_container': procedural_container,
            'sky_top_widget': sky_top_widget,
            'sky_horizon_widget': sky_horizon_widget,
            'sky_bottom_widget': sky_bottom_widget,
            'cubemap_container': cubemap_container,
            'cubemap_widgets': cubemap_widgets,
            'panoramic_container': panoramic_container,
            'panoramic_widget': panoramic_widget
        }

        # Connect header toggle
        header_btn.toggled.connect(lambda checked: self._toggle_category('skybox', checked))

    def _create_fog_category(self, parent_layout):
        """Create fog settings category"""
        # Header
        header_btn = self._create_category_header("Fog", False)
        parent_layout.addWidget(header_btn)

        # Content widget
        content_widget = QWidget()
        content_layout = QVBoxLayout()
        content_layout.setContentsMargins(10, 2, 0, 2)
        content_layout.setSpacing(2)
        content_widget.setLayout(content_layout)
        content_widget.setVisible(False)

        # Fog enabled
        fog_enabled_widget = BoolPropertyWidget("Fog Enabled", False)
        fog_enabled_widget.value_changed.connect(lambda v: self._set_property('fogEnabled', v))
        content_layout.addWidget(fog_enabled_widget)

        # Fog color
        fog_color_widget = ColorPropertyWidget("Fog Color", (0.5, 0.5, 0.5, 1.0))
        fog_color_widget.value_changed.connect(lambda v: self._set_property('fogColor', v))
        content_layout.addWidget(fog_color_widget)

        # Fog mode
        fog_mode_widget = EnumPropertyWidget("Fog Mode",
            ["Linear", "Exponential", "Exponential Squared"], 0)
        fog_mode_widget.value_changed.connect(lambda v: self._set_property('fogMode', v))
        content_layout.addWidget(fog_mode_widget)

        # Fog density
        fog_density_widget = self._create_float_widget("Fog Density", 0.01, 0.0, 1.0, 0.001)
        fog_density_widget.spinbox.valueChanged.connect(lambda v: self._set_property('fogDensity', v))
        content_layout.addWidget(fog_density_widget)

        # Fog start
        fog_start_widget = self._create_float_widget("Fog Start", 10.0, 0.0, 1000.0, 1.0)
        fog_start_widget.spinbox.valueChanged.connect(lambda v: self._set_property('fogStart', v))
        content_layout.addWidget(fog_start_widget)

        # Fog end
        fog_end_widget = self._create_float_widget("Fog End", 100.0, 0.0, 10000.0, 1.0)
        fog_end_widget.spinbox.valueChanged.connect(lambda v: self._set_property('fogEnd', v))
        content_layout.addWidget(fog_end_widget)

        parent_layout.addWidget(content_widget)

        # Store widgets
        self.category_widgets['fog'] = {
            'header': header_btn,
            'content': content_widget,
            'enabled_widget': fog_enabled_widget,
            'color_widget': fog_color_widget,
            'mode_widget': fog_mode_widget,
            'density_widget': fog_density_widget,
            'start_widget': fog_start_widget,
            'end_widget': fog_end_widget
        }

        # Connect header toggle
        header_btn.toggled.connect(lambda checked: self._toggle_category('fog', checked))

    def _create_ambient_light_category(self, parent_layout):
        """Create ambient light settings category"""
        # Header
        header_btn = self._create_category_header("Ambient Light", False)
        parent_layout.addWidget(header_btn)

        # Content widget
        content_widget = QWidget()
        content_layout = QVBoxLayout()
        content_layout.setContentsMargins(10, 2, 0, 2)
        content_layout.setSpacing(2)
        content_widget.setLayout(content_layout)
        content_widget.setVisible(False)

        # Ambient light enabled
        ambient_enabled_widget = BoolPropertyWidget("Ambient Light Enabled", True)
        ambient_enabled_widget.value_changed.connect(lambda v: self._set_property('ambientLightEnabled', v))
        content_layout.addWidget(ambient_enabled_widget)

        # Ambient light color
        ambient_color_widget = ColorPropertyWidget("Ambient Light Color", (1.0, 1.0, 1.0, 1.0))
        ambient_color_widget.value_changed.connect(lambda v: self._set_property('ambientLightColor', v))
        content_layout.addWidget(ambient_color_widget)

        # Ambient light intensity
        ambient_intensity_widget = self._create_float_widget("Ambient Light Intensity", 0.2, 0.0, 10.0, 0.01)
        ambient_intensity_widget.spinbox.valueChanged.connect(lambda v: self._set_property('ambientLightIntensity', v))
        content_layout.addWidget(ambient_intensity_widget)

        parent_layout.addWidget(content_widget)

        # Store widgets
        self.category_widgets['ambient'] = {
            'header': header_btn,
            'content': content_widget,
            'enabled_widget': ambient_enabled_widget,
            'color_widget': ambient_color_widget,
            'intensity_widget': ambient_intensity_widget
        }

        # Connect header toggle
        header_btn.toggled.connect(lambda checked: self._toggle_category('ambient', checked))

    def _create_volumetric_fog_category(self, parent_layout):
        """Create volumetric fog settings category (disabled for OpenGL)"""
        # Header
        header_btn = self._create_category_header("Volumetric Fog (Not Available in OpenGL)", False)
        parent_layout.addWidget(header_btn)

        # Content widget
        content_widget = QWidget()
        content_layout = QVBoxLayout()
        content_layout.setContentsMargins(10, 2, 0, 2)
        content_layout.setSpacing(2)
        content_widget.setLayout(content_layout)
        content_widget.setVisible(False)

        # Disabled message
        disabled_label = QLabel("Volumetric fog is not available in the OpenGL backend.")
        disabled_label.setWordWrap(True)
        disabled_label.setStyleSheet("color: gray; font-style: italic;")
        content_layout.addWidget(disabled_label)

        parent_layout.addWidget(content_widget)

        # Store widgets
        self.category_widgets['volumetric'] = {
            'header': header_btn,
            'content': content_widget
        }

        # Connect header toggle
        header_btn.toggled.connect(lambda checked: self._toggle_category('volumetric', checked))

    def _toggle_category(self, category_name, checked):
        """Toggle category visibility"""
        if category_name in self.category_widgets:
            widgets = self.category_widgets[category_name]
            widgets['content'].setVisible(checked)
            # Update arrow
            header = widgets['header']
            text = header.text()
            if checked:
                header.setText(text.replace('▶', '▼'))
            else:
                header.setText(text.replace('▼', '▶'))

    def _on_skybox_type_changed(self, skybox_type):
        """Handle skybox type change - show/hide appropriate fields"""
        if 'skybox' not in self.category_widgets:
            return

        widgets = self.category_widgets['skybox']

        # Hide all containers
        widgets['color_container'].setVisible(False)
        widgets['procedural_container'].setVisible(False)
        widgets['cubemap_container'].setVisible(False)
        widgets['panoramic_container'].setVisible(False)

        # Show appropriate container based on type
        if skybox_type == 1:  # Color
            widgets['color_container'].setVisible(True)
        elif skybox_type == 2:  # Procedural
            widgets['procedural_container'].setVisible(True)
        elif skybox_type == 3:  # Cubemap
            widgets['cubemap_container'].setVisible(True)
        elif skybox_type == 4:  # Panoramic
            widgets['panoramic_container'].setVisible(True)

        # Set property
        self._set_property('skyboxType', skybox_type)

    def _set_property(self, prop_name, value):
        """Set component property through editor bridge"""
        if self.component and self.editor_bridge:
            try:
                import json
                # Convert color tuples to dict format
                if isinstance(value, (tuple, list)) and len(value) == 4:
                    value_dict = {'r': float(value[0]), 'g': float(value[1]),
                                  'b': float(value[2]), 'a': float(value[3])}
                    self.editor_bridge.set_component_property(
                        self.component, prop_name, json.dumps(value_dict))
                else:
                    self.editor_bridge.set_component_property(
                        self.component, prop_name, json.dumps(value))
            except Exception as e:
                print(f"[Warning] Failed to set WorldEnvironment property {prop_name}: {e}")

    def set_value(self, value):
        """Load values from component"""
        if not self.component or not self.editor_bridge:
            return

        try:
            import json
            props_json = self.editor_bridge.get_component_properties(self.component)
            properties = json.loads(props_json) if props_json else {}
            props = properties.get('properties', {})

            # Helper function to convert color dict to tuple
            def color_to_tuple(color_dict):
                if isinstance(color_dict, dict):
                    return (color_dict.get('r', 0.0), color_dict.get('g', 0.0),
                            color_dict.get('b', 0.0), color_dict.get('a', 1.0))
                return (0.5, 0.5, 0.5, 1.0)

            # Load skybox properties
            if 'skybox' in self.category_widgets:
                skybox = self.category_widgets['skybox']

                # Skybox type
                skybox_type = props.get('skyboxType', 0)
                if 'type_widget' in skybox:
                    skybox['type_widget'].set_value(skybox_type)
                    # Update visibility of containers based on type
                    self._on_skybox_type_changed(skybox_type)

                # Color skybox
                if 'color_widget' in skybox and 'skyboxColor' in props:
                    skybox['color_widget'].set_value(color_to_tuple(props['skyboxColor']))

                # Procedural skybox
                if 'sky_top_widget' in skybox and 'skyTopColor' in props:
                    skybox['sky_top_widget'].set_value(color_to_tuple(props['skyTopColor']))
                if 'sky_horizon_widget' in skybox and 'skyHorizonColor' in props:
                    skybox['sky_horizon_widget'].set_value(color_to_tuple(props['skyHorizonColor']))
                if 'sky_bottom_widget' in skybox and 'skyBottomColor' in props:
                    skybox['sky_bottom_widget'].set_value(color_to_tuple(props['skyBottomColor']))

                # Cubemap skybox
                if 'cubemap_widgets' in skybox:
                    for face_key, widget in skybox['cubemap_widgets'].items():
                        prop_key = f'cubemap{face_key.capitalize()}'
                        if prop_key in props:
                            widget.set_value(props[prop_key])

                # Panoramic skybox
                if 'panoramic_widget' in skybox and 'panoramicTexture' in props:
                    skybox['panoramic_widget'].set_value(props['panoramicTexture'])

            # Load fog properties
            if 'fog' in self.category_widgets:
                fog = self.category_widgets['fog']

                if 'enabled_widget' in fog and 'fogEnabled' in props:
                    fog['enabled_widget'].set_value(props['fogEnabled'])
                if 'color_widget' in fog and 'fogColor' in props:
                    fog['color_widget'].set_value(color_to_tuple(props['fogColor']))
                if 'mode_widget' in fog and 'fogMode' in props:
                    fog['mode_widget'].set_value(props['fogMode'])
                if 'density_widget' in fog and 'fogDensity' in props:
                    fog['density_widget'].spinbox.setValue(props['fogDensity'])
                if 'start_widget' in fog and 'fogStart' in props:
                    fog['start_widget'].spinbox.setValue(props['fogStart'])
                if 'end_widget' in fog and 'fogEnd' in props:
                    fog['end_widget'].spinbox.setValue(props['fogEnd'])

            # Load ambient light properties
            if 'ambient' in self.category_widgets:
                ambient = self.category_widgets['ambient']

                if 'enabled_widget' in ambient and 'ambientLightEnabled' in props:
                    ambient['enabled_widget'].set_value(props['ambientLightEnabled'])
                if 'color_widget' in ambient and 'ambientLightColor' in props:
                    ambient['color_widget'].set_value(color_to_tuple(props['ambientLightColor']))
                if 'intensity_widget' in ambient and 'ambientLightIntensity' in props:
                    ambient['intensity_widget'].spinbox.setValue(props['ambientLightIntensity'])

        except Exception as e:
            print(f"[Warning] Failed to load WorldEnvironment values: {e}")

    def get_value(self):
        """Get current values"""
        return None


class InspectorPanel(EditorPanel):
    """Properties inspector panel for editing object properties"""

    # Signals
    property_changed = pyqtSignal(str, object, object)  # property name, old value, new value
    component_added = pyqtSignal(str)  # component type
    component_removed = pyqtSignal(object)  # component instance

    def __init__(self, parent=None):
        super().__init__("Inspector", parent)
        self.setObjectName("InspectorPanel")
        self.main_editor = parent
        self.editor_bridge = None
        self.current_node = None
        self.current_archetype_path = None
        self._archetype_self_saves = {}
        self.property_widgets = {}
        self.update_timer = QTimer()
        self.update_timer.setSingleShot(True)
        self.update_timer.timeout.connect(self._apply_name_change)
        self._active_connections = []  # Track active signal connections
        self._material_widgets = {}  # Track material widgets per component for refresh
        self._shader_attachment_widgets = []  # Track ColorRect shader widgets for refresh on save
        # Persist component card collapse state across refreshes (e.g. adding a
        # component) keyed by (type_name, occurrence_index). Cleared when the
        # inspected node changes so a new selection starts fully expanded.
        self._component_card_expanded = {}
        self._card_type_counter = {}
        # Parsed get_node_properties() result, memoized for the lifetime of a single
        # refresh so the Transform and node-type sections don't each re-query the
        # native bridge. Reset to None at the start of every refresh_properties().
        self._node_props_cache = None

        # Multi-node (shared) property editing. When more than one node is
        # selected the inspector shows only the properties common to the whole
        # selection - including inherited ancestor properties such as the
        # UIControl base shared by a Label and a Button - and broadcasts every
        # edit to all of them. current_node remains the primary (first-selected)
        # node and drives the displayed values; the rest ride along through the
        # broadcast handlers.
        self.selected_nodes = []
        self._multi_mode = False
        self._shared_node_prop_names = None  # None => single-node mode (no filtering)
        self._node_props_per_node = []       # parsed node properties aligned to selected_nodes
        self._shared_component_groups = []   # group descriptors rendered in multi mode
        self._component_peers = {}           # id(primary component) -> peer components per node

    def _setup_panel(self):
        """Setup inspector panel UI"""
        t = inspector_tokens()

        # Identity bar: eyebrow + the editable node name, on a recessed surface
        header_frame = QFrame()
        header_frame.setObjectName("inspectorIdentity")
        header_frame.setStyleSheet(f"""
            QFrame#inspectorIdentity {{
                background-color: {t.background};
                border-bottom: 1px solid {t.border};
            }}
        """)
        header_layout = QVBoxLayout(header_frame)
        header_layout.setContentsMargins(10, 8, 10, 8)
        header_layout.setSpacing(3)

        name_label = QLabel("SELECTED NODE")
        name_label.setStyleSheet(
            f"QLabel {{ color: {with_alpha(t.text_secondary, 0.8)}; background: transparent;"
            f" font-size: 8px; font-weight: bold; letter-spacing: 1.5px; }}")
        header_layout.addWidget(name_label)

        self.node_name_edit = QLineEdit()
        self.node_name_edit.setPlaceholderText("No Selection")
        self.node_name_edit.setEnabled(False)
        self.node_name_edit.textChanged.connect(self._on_name_changed)
        self.node_name_edit.setStyleSheet(f"""
            QLineEdit {{
                background-color: {t.surface};
                color: {t.text_primary};
                border: 1px solid {t.border};
                border-radius: 4px;
                padding: 6px 10px;
                font-size: 13px;
                font-weight: 600;
            }}
            QLineEdit:focus {{ border-color: {t.border_focus}; }}
            QLineEdit:disabled {{ color: {t.text_disabled}; font-weight: 500; }}
        """)
        header_layout.addWidget(self.node_name_edit)

        # Scrollable properties area with horizontal scrolling support
        scroll_area = QScrollArea()
        scroll_area.setWidgetResizable(True)
        scroll_area.setFrameShape(QScrollArea.Shape.NoFrame)
        # Allow horizontal scrolling when panel is too narrow
        scroll_area.setHorizontalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAsNeeded)

        self.properties_widget = QWidget()
        self.properties_layout = QVBoxLayout()
        v_spacing, h_padding = get_theme_spacing()
        self.properties_layout.setContentsMargins(h_padding, h_padding, h_padding, h_padding)
        self.properties_layout.setSpacing(v_spacing)
        self.properties_widget.setLayout(self.properties_layout)

        scroll_area.setWidget(self.properties_widget)

        # Add to layout
        self.content_layout.addWidget(header_frame)
        self.content_layout.addWidget(scroll_area)

    def set_node(self, node):
        """Set a single node to inspect (clears any active multi-selection)."""
        self.set_nodes([node] if node is not None else [])

    def set_nodes(self, nodes):
        """Set the node(s) to inspect.

        A single node behaves exactly as before. With two or more nodes the
        inspector enters shared-editing mode: only properties common to every
        selected node (including inherited ancestor properties, e.g. the UIControl
        base of a Label or Button) are shown, and each edit is applied to all of
        the selected nodes at once.
        """
        unique_nodes = []
        seen_keys = set()
        for node in (nodes or []):
            if node is None:
                continue
            key = self._node_key(node)
            if key in seen_keys:
                continue
            seen_keys.add(key)
            unique_nodes.append(node)

        primary = unique_nodes[0] if unique_nodes else None
        if primary is not self.current_node:
            self._component_card_expanded.clear()

        self.selected_nodes = unique_nodes
        self._multi_mode = len(unique_nodes) > 1
        self.current_node = primary
        self.current_archetype_path = None
        self.refresh_properties()

    def _node_key(self, node):
        """Stable identity for a node used to de-duplicate a selection list."""
        try:
            return node.get_uuid().to_string()
        except Exception:
            return id(node)

    def set_archetype_instance(self, path):
        """Inspect an archetype instance asset (.ares) instead of a scene node."""
        self._component_card_expanded.clear()
        self.current_node = None
        self.current_archetype_path = path
        self._refresh_archetype()

    def _refresh_archetype(self):
        """Render the editable fields of the current archetype instance."""
        for connection in self._active_connections:
            try:
                connection[0].disconnect(connection[1])
            except Exception:
                pass
        self._active_connections.clear()
        self._material_widgets.clear()
        self._shader_attachment_widgets.clear()
        self.property_widgets.clear()
        self._clear_layout(self.properties_layout)

        if not self.current_archetype_path or not self.editor_bridge:
            self.node_name_edit.setText("")
            self.node_name_edit.setEnabled(False)
            self.node_name_edit.setPlaceholderText("No Selection")
            return

        try:
            data = json.loads(self.editor_bridge.load_archetype_instance(self.current_archetype_path))
        except Exception as e:
            data = None
            if self.main_editor and 'console' in self.main_editor.panels:
                self.main_editor.panels['console'].log_message(
                    f"Failed to load archetype instance: {str(e)}", "Error")

        if not data:
            error_label = QLabel("Could not load archetype instance.")
            error_label.setStyleSheet(f"color: {inspector_tokens().error};")
            self.properties_layout.addWidget(error_label)
            self.properties_layout.addStretch()
            return

        class_name = data.get("archetype_class", "Archetype")
        definition_path = data.get("definition", "")
        properties = data.get("properties", {}) or {}
        metadata = data.get("property_metadata", {}) or {}

        self.node_name_edit.blockSignals(True)
        self.node_name_edit.setText(Path(self.current_archetype_path).stem)
        self.node_name_edit.setEnabled(False)
        self.node_name_edit.setPlaceholderText("")
        self.node_name_edit.blockSignals(False)

        tok = inspector_tokens()
        card = InspectorCard(class_name, eyebrow="Archetype")
        group_layout = card.body_layout

        if definition_path:
            def_layout = QHBoxLayout()
            def_label = QLabel(f"Definition: {definition_path}")
            def_label.setWordWrap(True)
            def_label.setStyleSheet(
                f"color: {tok.text_disabled}; font-style: italic; background: transparent;")
            def_layout.addWidget(def_label, 1)
            open_def_btn = QPushButton("Open Definition")
            open_def_btn.setFixedWidth(130)
            open_def_btn.clicked.connect(
                lambda checked=False, p=definition_path: self._open_archetype_definition(p))
            def_layout.addWidget(open_def_btn)
            group_layout.addLayout(def_layout)

        # Track identity/required metadata for instance-level validation.
        self._archetype_class = class_name
        self._archetype_unique_fields = set()
        required_field_names = []
        for prop_name, field_meta in metadata.items():
            usage = self._usage_flags(field_meta)
            if usage & USAGE_UNIQUE:
                self._archetype_unique_fields.add(prop_name)
            if usage & USAGE_REQUIRED:
                required_field_names.append(prop_name)

        missing_required = [
            name for name in required_field_names
            if self._is_empty_value(properties.get(name))
        ]
        if missing_required:
            banner = QLabel("Required fields are empty: " + ", ".join(sorted(missing_required)))
            banner.setWordWrap(True)
            banner.setStyleSheet(
                f"color: {tok.error}; font-weight: 600; background: transparent;")
            group_layout.addWidget(banner)

        grouped_fields = {}
        for prop_name, prop_value in properties.items():
            field_meta = metadata.get(prop_name, {})
            usage = self._usage_flags(field_meta)
            field_type = field_meta.get("type")
            field_group = field_meta.get("group", "") or ""
            widget = self._create_property_widget(prop_name, prop_value, field_meta)
            if not widget:
                continue
            if usage & USAGE_READONLY:
                widget.setEnabled(False)
            suffix = field_meta.get("suffix")
            if suffix:
                self._apply_suffix(widget, suffix)
            self._track_connection(
                widget.value_changed,
                lambda v, n=prop_name, t=field_type: self._on_archetype_field_changed(n, v, t)
            )
            target_list = grouped_fields.setdefault(field_group, []) if field_group else None
            header = field_meta.get("header")
            if header:
                header_label = self._make_header_label(header)
                if target_list is not None:
                    target_list.append(header_label)
                else:
                    group_layout.addWidget(header_label)
            if target_list is not None:
                target_list.append(widget)
            else:
                group_layout.addWidget(widget)

        self.properties_layout.addWidget(card)

        for field_group in sorted(grouped_fields.keys()):
            collapsible_group = CollapsibleGroupBox(field_group)
            for widget in grouped_fields[field_group]:
                collapsible_group.add_widget(widget)
            self.properties_layout.addWidget(collapsible_group)

        self.properties_layout.addStretch()

    def _open_archetype_definition(self, definition_path):
        """Ask the main editor to open the archetype's definition file."""
        if self.main_editor and hasattr(self.main_editor, 'open_archetype_definition'):
            self.main_editor.open_archetype_definition(definition_path)

    def _archetype_value_to_obj(self, value, field_type):
        """Convert a property widget value into a JSON-serializable field value."""
        if isinstance(value, dict) and COLLECTION_SENTINEL_KEY in value:
            return value[COLLECTION_SENTINEL_KEY]
        if isinstance(value, bool):
            return value
        if isinstance(value, (tuple, list)):
            values = list(value)

            def floats(count, fallback):
                result = [float(v) for v in values[:count]]
                while len(result) < count:
                    result.append(fallback)
                return result

            if field_type == 4:
                v = floats(2, 0.0)
                return {"x": v[0], "y": v[1]}
            if field_type == 5:
                v = floats(3, 0.0)
                return {"x": v[0], "y": v[1], "z": v[2]}
            if field_type == 6:
                v = floats(4, 0.0)
                return {"x": v[0], "y": v[1], "z": v[2], "w": v[3]}
            if field_type == 7:
                v = floats(4, 1.0)
                return {"r": v[0], "g": v[1], "b": v[2], "a": v[3]}
            if field_type == 11:
                return [str(v) for v in values]
            if len(values) == 2:
                return {"x": float(values[0]), "y": float(values[1])}
            if len(values) == 3:
                return {"x": float(values[0]), "y": float(values[1]), "z": float(values[2])}
            if len(values) == 4:
                v = [float(x) for x in values]
                return {"x": v[0], "y": v[1], "z": v[2], "w": v[3]}
            return values
        return value

    def _on_archetype_field_changed(self, name, value, field_type):
        """Persist a changed archetype field value back to the .ares file."""
        if not self.editor_bridge or not self.current_archetype_path:
            return
        try:
            field_value = self._archetype_value_to_obj(value, field_type)
            self.editor_bridge.save_archetype_instance(
                self.current_archetype_path, json.dumps({name: field_value}))
            self._note_archetype_self_save(self.current_archetype_path)
        except Exception as e:
            if self.main_editor and 'console' in self.main_editor.panels:
                self.main_editor.panels['console'].log_message(
                    f"Failed to save archetype field '{name}': {str(e)}", "Error")
            return

        # Identity ([Unique]) fields must not collide with a sibling instance.
        if name in getattr(self, '_archetype_unique_fields', set()):
            conflict = self._archetype_unique_conflict(name, field_value)
            if conflict and self.main_editor and 'console' in self.main_editor.panels:
                self.main_editor.panels['console'].log_message(
                    f"Archetype field '{name}' value '{field_value}' is not unique - "
                    f"also used by '{conflict}'.", "Warning")

    @staticmethod
    def _normalize_archetype_path(path):
        """Normalize an archetype path for cross-source comparison (watcher vs inspector)."""
        if not path:
            return ""
        try:
            return os.path.normcase(os.path.abspath(str(path)))
        except Exception:
            return os.path.normcase(str(path))

    def _note_archetype_self_save(self, path):
        """Record that the inspector itself just wrote this .ares, so the file
        watcher's echo of that write does not trigger a redundant UI rebuild."""
        import time
        self._archetype_self_saves[self._normalize_archetype_path(path)] = time.time()

    def refresh_archetype_if_external(self, path):
        """Re-read the currently-inspected .ares when it changed on disk from an
        external source (a running play instance, another tool). Inspector-initiated
        saves are ignored within a short window to avoid a save -> rebuild feedback loop."""
        if not self.current_archetype_path:
            return
        changed = self._normalize_archetype_path(path)
        if changed != self._normalize_archetype_path(self.current_archetype_path):
            return
        import time
        last_self = self._archetype_self_saves.get(changed, 0.0)
        if time.time() - last_self < 1.5:
            return
        self._refresh_archetype()

    @staticmethod
    def _is_empty_value(value):
        """Whether an archetype field value counts as empty for [Required] checks."""
        if value is None:
            return True
        if isinstance(value, str):
            return value.strip() == ""
        if isinstance(value, (list, dict)):
            return len(value) == 0
        return False

    def _archetype_unique_conflict(self, field_name, value):
        """Return a sibling instance path that already uses ``value`` for ``field_name``.

        Compares against every other instance of the same archetype class; returns the
        first conflicting instance's path, or None when the value is unique.
        """
        class_name = getattr(self, '_archetype_class', "")
        if not class_name or self._is_empty_value(value):
            return None
        try:
            raw = self.editor_bridge.find_archetype_instances(class_name)
            instances = json.loads(raw) if isinstance(raw, str) else (raw or [])
        except Exception:
            return None
        current = self.current_archetype_path or ""
        for entry in instances:
            if not isinstance(entry, dict):
                continue
            path = entry.get("path", "")
            if not path or path == current:
                continue
            try:
                other = json.loads(self.editor_bridge.load_archetype_instance(path))
            except Exception:
                continue
            other_value = (other or {}).get("properties", {}).get(field_name)
            if other_value == value:
                return path
        return None

    def on_shader_saved(self, shader_path: str):
        """Re-introspect parameters for any displayed ColorRect using the saved shader."""
        for widget in list(self._shader_attachment_widgets):
            try:
                widget.refresh_shader(shader_path)
            except RuntimeError:
                # Widget was deleted (inspector rebuilt); ignore.
                pass

    def _clear_layout(self, layout):
        """Recursively clear a layout and delete all contained widgets and sub-layouts."""
        while layout.count():
            item = layout.takeAt(0)
            widget = item.widget()
            child_layout = item.layout()
            if widget is not None:
                # Disconnect all signals from the widget and its children before deleting
                self._disconnect_widget_signals(widget)
                widget.deleteLater()
            elif child_layout is not None:
                self._clear_layout(child_layout)
    
    def _disconnect_widget_signals(self, widget):
        """Disconnect common signals from a widget and all of its descendants.

        ``QWidget.findChildren(QWidget)`` already returns the entire descendant subtree
        in one flat list, so a single pass covers everything. The previous version
        recursed per child and called ``findChildren`` again on each, re-walking every
        subtree repeatedly (quadratic) - that dominated inspector teardown on nodes
        with many widgets and was a primary cause of the selection lag.
        """
        self._disconnect_one_widget_signals(widget)
        try:
            for child in widget.findChildren(QWidget):
                self._disconnect_one_widget_signals(child)
        except Exception:
            pass

    @staticmethod
    def _disconnect_one_widget_signals(widget):
        """Disconnect the common editable-widget signals of a single widget."""
        for signal_name in ('value_changed', 'valueChanged', 'textChanged',
                            'clicked', 'editingFinished'):
            signal = getattr(widget, signal_name, None)
            if signal is None:
                continue
            try:
                signal.disconnect()
            except Exception:
                pass
    
    def _track_connection(self, signal, slot):
        """Connect a signal to a slot and track it for later disconnection"""
        signal.connect(slot)
        self._active_connections.append((signal, slot))

    def refresh_properties(self):
        """Refresh all properties from the current node.

        The full property tree is torn down and rebuilt inside a single suppressed-
        repaint window so the panel paints once at the end instead of once per added
        widget. This is what keeps node selection feeling near-instant even on nodes
        with many components and properties.
        """
        # Reset the per-refresh cache shared by the Transform and node-type sections
        # so the native get_node_properties bridge is queried only once per selection.
        self._node_props_cache = None
        self.properties_widget.setUpdatesEnabled(False)
        try:
            self._rebuild_properties()
        finally:
            self.properties_widget.setUpdatesEnabled(True)

    def _rebuild_properties(self):
        """Tear down the previous property widgets and rebuild for the current node."""
        # Disconnect all tracked connections before clearing
        for connection in self._active_connections:
            try:
                connection[0].disconnect(connection[1])
            except:
                pass
        self._active_connections.clear()

        # Clear material widgets tracking
        self._material_widgets.clear()
        self._shader_attachment_widgets.clear()
        # Reset per-type occurrence counters used to key card collapse state
        self._card_type_counter.clear()

        # Clear existing widgets
        self.property_widgets.clear()
        self._clear_layout(self.properties_layout)

        if not self.current_node:
            self.node_name_edit.setText("")
            self.node_name_edit.setEnabled(False)
            self.node_name_edit.setPlaceholderText("No Selection")
            self._clear_multi_selection_model()
            return

        # Resolve the shared-property model before any section is built so the
        # builders can filter to the common properties and flag mixed values.
        if self._multi_mode:
            self._compute_multi_selection_model()
        else:
            self._clear_multi_selection_model()

        # Set node name. A multi-selection has no single editable name, so the
        # field becomes a read-only count.
        self.node_name_edit.blockSignals(True)
        if self._multi_mode:
            self.node_name_edit.setText(f"{len(self.selected_nodes)} nodes selected")
            self.node_name_edit.setEnabled(False)
        else:
            self.node_name_edit.setText(self.current_node.get_name())
            self.node_name_edit.setEnabled(True)
        self.node_name_edit.setPlaceholderText("")
        self.node_name_edit.blockSignals(False)

        # Add Node Properties section
        self._add_node_properties_section()

        # Add Transform section (for Node2D/Node3D)
        self._add_transform_section()

        # Add the node's own registered properties (beyond transform), e.g. Camera2D
        # zoom/ortho_size or CameraUI canvas_size/zoom.
        self._add_node_type_properties_section()

        # Add components sections
        self._add_components_section()

        # Add stretch at the end
        self.properties_layout.addStretch()

    # ------------------------------------------------------------------
    # Shared multi-node selection model
    # ------------------------------------------------------------------

    @staticmethod
    def _value_kind(value):
        """Coarse structural classifier so a property is only treated as shared
        when its value has the same shape across the whole selection (e.g. a float
        rotation must never merge with a quaternion rotation)."""
        if isinstance(value, bool):
            return "bool"
        if isinstance(value, int):
            return "int"
        if isinstance(value, float):
            return "float"
        if isinstance(value, str):
            return "str"
        if isinstance(value, dict):
            keys = set(value.keys())
            if {"x", "y", "z", "w"}.issubset(keys):
                return "vec4"
            if {"x", "y", "z"}.issubset(keys):
                return "vec3"
            if {"x", "y"}.issubset(keys):
                return "vec2"
            if {"r", "g", "b"}.issubset(keys):
                return "color"
            return "dict"
        if isinstance(value, (list, tuple)):
            return "list"
        if value is None:
            return "none"
        return "other"

    @staticmethod
    def _value_signature(value):
        """A stable, hashable signature for cross-selection equality comparison."""
        try:
            return json.dumps(value, sort_keys=True)
        except (TypeError, ValueError):
            return repr(value)

    def _clear_multi_selection_model(self):
        """Reset all shared-selection bookkeeping (single-node / empty selection)."""
        self._shared_node_prop_names = None
        self._node_props_per_node = []
        self._shared_component_groups = []
        self._component_peers = {}

    def _node_properties_dict(self, node):
        """Parsed {name: value} registered-property map for one node (empty on error)."""
        if not node or not self.editor_bridge:
            return {}
        try:
            raw = self.editor_bridge.get_node_properties(node)
            data = json.loads(raw) if isinstance(raw, str) else (raw or {})
        except Exception:
            return {}
        props = data.get("properties", {}) if isinstance(data, dict) else {}
        return props if isinstance(props, dict) else {}

    def _component_properties_dict(self, component):
        """Parsed {name: value} property map for one component (empty on error)."""
        if not component or not self.editor_bridge:
            return {}
        try:
            raw = self.editor_bridge.get_component_properties(component)
            data = json.loads(raw) if isinstance(raw, str) else (raw or {})
        except Exception:
            return {}
        props = data.get("properties", {}) if isinstance(data, dict) else {}
        return props if isinstance(props, dict) else {}

    @staticmethod
    def _component_type_chain(component):
        """Most-derived-first list of type names for a component (own type as a
        fallback when the runtime predates get_type_chain)."""
        try:
            chain = list(component.get_type_chain())
            if chain:
                return chain
        except Exception:
            pass
        try:
            return [component.get_type_name()]
        except Exception:
            return []

    def _compute_multi_selection_model(self):
        """Build the shared-property model for the current multi-node selection:
        which node-level properties are common to every node, and which component
        groups (exact-type matches plus shared ancestor types) can be edited
        together. Populates the bookkeeping consumed by the section builders and
        the broadcast handlers."""
        self._component_peers = {}
        nodes = self.selected_nodes

        # --- Shared node-level properties -----------------------------------
        self._node_props_per_node = [self._node_properties_dict(n) for n in nodes]
        primary_props = self._node_props_per_node[0] if self._node_props_per_node else {}
        shared_names = None
        for props in self._node_props_per_node:
            keys = set(props.keys())
            shared_names = keys if shared_names is None else (shared_names & keys)
        shared_names = shared_names or set()
        compatible = set()
        for name in shared_names:
            kind = self._value_kind(primary_props.get(name))
            if all(self._value_kind(p.get(name)) == kind for p in self._node_props_per_node):
                compatible.add(name)
        self._shared_node_prop_names = compatible

        # --- Shared component groups ----------------------------------------
        self._shared_component_groups = self._build_shared_component_groups(nodes)

    def _build_shared_component_groups(self, nodes):
        """Match components across the selection into shared, co-editable groups.

        Two passes:
          1. Exact match by (type_name, occurrence_index) - covers the common case
             of selecting several nodes of the same kind (e.g. five Labels).
          2. Shared-ancestor match for the leftover components - finds the most
             derived type that every node still has an unconsumed component for
             (e.g. a Label and a Button share their UIControl base) so inherited
             properties stay editable across a mixed-type selection.
        """
        node_components = []
        for node in nodes:
            try:
                node_components.append(list(node.get_components()))
            except Exception:
                node_components.append([])

        # Per-node occurrence map: (type_name, occurrence_index) -> component.
        occ_maps = []
        for comps in node_components:
            counts = {}
            occ_map = {}
            for comp in comps:
                try:
                    type_name = comp.get_type_name()
                except Exception:
                    continue
                occ = counts.get(type_name, 0)
                counts[type_name] = occ + 1
                occ_map[(type_name, occ)] = comp
            occ_maps.append(occ_map)

        consumed = [set() for _ in nodes]
        groups = []

        # Pass 1: exact (type, occurrence) present on every node.
        primary_counts = {}
        for comp in node_components[0]:
            try:
                type_name = comp.get_type_name()
            except Exception:
                continue
            occ = primary_counts.get(type_name, 0)
            primary_counts[type_name] = occ + 1
            key = (type_name, occ)
            if all(key in occ_maps[i] for i in range(len(nodes))):
                peers = [occ_maps[i][key] for i in range(len(nodes))]
                group = self._make_component_group(type_name, peers, kind="exact")
                if group is not None:
                    groups.append(group)
                    for i, peer in enumerate(peers):
                        consumed[i].add(id(peer))

        # Pass 2: shared ancestor for each leftover primary component.
        for comp in node_components[0]:
            if id(comp) in consumed[0]:
                continue
            chain = self._component_type_chain(comp)
            chosen_peers = None
            chosen_type = None
            for type_name in chain:
                if type_name == "Component":
                    continue
                peers = [comp]
                ok = True
                for i in range(1, len(nodes)):
                    candidate = None
                    for other in node_components[i]:
                        if id(other) in consumed[i]:
                            continue
                        if type_name in self._component_type_chain(other):
                            candidate = other
                            break
                    if candidate is None:
                        ok = False
                        break
                    peers.append(candidate)
                if ok:
                    chosen_peers = peers
                    chosen_type = type_name
                    break
            if chosen_peers is not None:
                group = self._make_component_group(chosen_type, chosen_peers, kind="ancestor")
                if group is not None:
                    groups.append(group)
                    for i, peer in enumerate(chosen_peers):
                        consumed[i].add(id(peer))

        return groups

    def _make_component_group(self, title, peers, kind):
        """Compute the shared/mixed property sets for a matched component group and
        register its peers for broadcast. Returns None when no editable property is
        common to the peers."""
        peer_props = [self._component_properties_dict(peer) for peer in peers]
        shared = None
        for props in peer_props:
            keys = set(props.keys())
            shared = keys if shared is None else (shared & keys)
        shared = shared or set()
        shared.discard("type")
        shared.discard("enabled")
        primary_props = peer_props[0] if peer_props else {}
        allowed = set()
        for name in shared:
            kind_primary = self._value_kind(primary_props.get(name))
            if all(self._value_kind(p.get(name)) == kind_primary for p in peer_props):
                allowed.add(name)
        if not allowed:
            return None
        mixed = set()
        for name in allowed:
            signatures = {self._value_signature(p.get(name)) for p in peer_props}
            if len(signatures) > 1:
                mixed.add(name)
        primary = peers[0]
        self._component_peers[id(primary)] = list(peers)
        return {
            "primary": primary,
            "peers": list(peers),
            "title": title,
            "allowed": allowed,
            "mixed": mixed,
            "kind": kind,
        }

    def _node_targets(self):
        """Nodes an edit should be applied to (the whole selection in multi mode)."""
        if self._multi_mode and self.selected_nodes:
            return list(self.selected_nodes)
        return [self.current_node] if self.current_node else []

    def _component_targets(self, component):
        """List of (component, owner_node) the displayed component maps to. In multi
        mode this expands to the matched peer component on every selected node."""
        if self._multi_mode:
            peers = self._component_peers.get(id(component))
            if peers and len(peers) == len(self.selected_nodes):
                return list(zip(peers, self.selected_nodes))
        return [(component, self.current_node)]

    def _is_shared_node_prop(self, name):
        """Whether a node-level property should be shown for the current selection."""
        return self._shared_node_prop_names is None or name in self._shared_node_prop_names

    def _node_prop_is_mixed(self, name):
        """Whether a node-level property holds differing values across the selection."""
        if not self._multi_mode:
            return False
        signatures = {self._value_signature(p.get(name)) for p in self._node_props_per_node}
        return len(signatures) > 1

    def _node_base_is_mixed(self, getter):
        """Whether a node accessor (is_active/is_visible/...) differs across selection."""
        if not self._multi_mode:
            return False
        values = set()
        for node in self.selected_nodes:
            try:
                values.add(getter(node))
            except Exception:
                pass
        return len(values) > 1

    @staticmethod
    def _mark_widget_mixed(widget, mixed):
        """Flag a property widget as holding multiple values (no-op if unsupported)."""
        if mixed and hasattr(widget, "set_mixed"):
            try:
                widget.set_mixed(True)
            except Exception:
                pass

    def _broadcast_node_active(self, value):
        for node in self._node_targets():
            try:
                node.set_active(value)
            except Exception:
                pass

    def _broadcast_node_visible(self, value):
        for node in self._node_targets():
            try:
                node.set_visible(value)
            except Exception:
                pass

    def _broadcast_unique_name(self, value):
        """Apply the unique-name flag to every selected node (multi mode). Per-node
        collision checking is skipped here; the single-node path keeps its dialog."""
        for node in self._node_targets():
            try:
                node.set_unique_name_in_owner(value)
            except Exception:
                pass
        if self.editor_bridge:
            self.editor_bridge.mark_scene_dirty()
        if self.main_editor and 'scene_tree' in self.main_editor.panels:
            self.main_editor.panels['scene_tree'].refresh_tree()

    def _set_components_enabled_multi(self, peers, value):
        for comp in peers:
            try:
                comp.set_enabled(value)
            except Exception:
                pass
        if self.editor_bridge:
            self.editor_bridge.mark_scene_dirty()

    def _remove_components_multi(self, peers):
        """Remove the matched component from every selected node (multi mode)."""
        if not self.editor_bridge:
            return
        removed = False
        for comp, owner in zip(peers, self.selected_nodes):
            try:
                if self.editor_bridge.remove_component(owner, comp):
                    removed = True
                    self.component_removed.emit(comp)
            except Exception:
                pass
        if removed:
            self.editor_bridge.mark_scene_dirty()
            self.refresh_properties()

    def _add_node_properties_section(self):
        """Add node properties section"""
        card = InspectorCard("Node", eyebrow="Base")

        # Active checkbox
        active_widget = BoolPropertyWidget("Active")
        active_widget.set_value(self.current_node.is_active())
        self._track_connection(active_widget.value_changed,
                               lambda v: self._broadcast_node_active(v))
        self._mark_widget_mixed(active_widget, self._node_base_is_mixed(lambda n: n.is_active()))
        card.add_body_widget(active_widget)

        # Visible checkbox
        visible_widget = BoolPropertyWidget("Visible")
        visible_widget.set_value(self.current_node.is_visible())
        self._track_connection(visible_widget.value_changed,
                               lambda v: self._broadcast_node_visible(v))
        self._mark_widget_mixed(visible_widget, self._node_base_is_mixed(lambda n: n.is_visible()))
        card.add_body_widget(visible_widget)

        # Unique Name (%) checkbox - exposes the node for %Name access in scripts
        unique_widget = BoolPropertyWidget("Unique Name (%)")
        unique_widget.set_value(self.current_node.is_unique_name_in_owner())
        if self._multi_mode:
            self._track_connection(unique_widget.value_changed,
                                   lambda v: self._broadcast_unique_name(v))
        else:
            self._track_connection(unique_widget.value_changed,
                                   lambda v, w=unique_widget: self._on_unique_name_changed(v, w))
        self._mark_widget_mixed(
            unique_widget, self._node_base_is_mixed(lambda n: n.is_unique_name_in_owner()))
        card.add_body_widget(unique_widget)

        # Path (read-only)
        t = inspector_tokens()
        path_layout = QHBoxLayout()
        path_layout.setContentsMargins(0, 2, 0, 0)
        path_layout.setSpacing(t.h_gap)
        path_label = make_property_label("Path")
        path_text = (f"{len(self.selected_nodes)} nodes selected"
                     if self._multi_mode else self.current_node.get_path())
        path_value = QLabel(path_text)
        path_value.setWordWrap(True)
        path_value.setTextInteractionFlags(Qt.TextInteractionFlag.TextSelectableByMouse)
        path_value.setStyleSheet(
            f"QLabel {{ color: {t.text_disabled}; background: transparent; font-style: italic; }}")
        path_layout.addWidget(path_label)
        path_layout.addWidget(path_value, 1)
        path_layout.setAlignment(path_label, Qt.AlignmentFlag.AlignTop)
        card.add_body_layout(path_layout)

        self.properties_layout.addWidget(card)

    def _on_unique_name_changed(self, value, widget):
        """Apply the 'Access as Unique Name' flag, warning on name collisions."""
        if not self.current_node:
            return
        if value:
            # Before this node becomes unique, resolving its name finds any OTHER
            # unique node sharing the name within scope; a hit means a collision.
            existing = self.current_node.resolve_unique_name(self.current_node.get_name())
            if existing is not None:
                QMessageBox.warning(
                    self, "Unique Name Conflict",
                    f"Another node named '{self.current_node.get_name()}' is already "
                    f"marked as a unique name within this scope. Unique names must be "
                    f"unique per scene.")
                widget.set_value(False)
                return
        self.current_node.set_unique_name_in_owner(value)
        if self.editor_bridge:
            self.editor_bridge.mark_scene_dirty()
        # Refresh the scene tree so the "%" badge reflects the new state.
        if self.main_editor and 'scene_tree' in self.main_editor.panels:
            self.main_editor.panels['scene_tree'].refresh_tree()

    def _get_node_properties_data(self):
        """Fetch and parse the current node's registered properties once per refresh.

        Both the Transform and node-type property sections need this data; memoizing
        the parsed result for the lifetime of a single refresh avoids a duplicate
        native bridge call plus JSON parse on every selection. Returns a dict (empty
        when there is no node/bridge or the bridge result can't be parsed).
        """
        if self._node_props_cache is not None:
            return self._node_props_cache
        data = {}
        if self.current_node and self.editor_bridge:
            try:
                properties_json = self.editor_bridge.get_node_properties(self.current_node)
                # `get_node_properties` returns a JSON string; be defensive if it's already parsed
                if isinstance(properties_json, str):
                    parsed = json.loads(properties_json)
                else:
                    parsed = properties_json or {}
                if isinstance(parsed, dict):
                    data = parsed
            except Exception:
                data = {}
        self._node_props_cache = data
        return data

    def _add_transform_section(self):
        """Add transform properties section (position/rotation/scale/z-index).

        This queries the node's registered properties via the editor bridge and, if
        transform-like properties are present, shows them in a dedicated group.
        """
        if not self.current_node or not self.editor_bridge:
            return

        try:
            data = self._get_node_properties_data()

            properties = data.get("properties", {})
            if not isinstance(properties, dict):
                return

            # Extract potential transform properties
            position = properties.get("position")
            rotation = properties.get("rotation")
            scale = properties.get("scale")
            z_index = properties.get("z_index")

            # In multi-selection mode, drop any transform field not shared by the
            # whole selection (e.g. a Node2D + Node3D mix won't share a rotation
            # shape) so only co-editable fields are shown.
            if self._multi_mode:
                if not self._is_shared_node_prop("position"):
                    position = None
                if not self._is_shared_node_prop("rotation"):
                    rotation = None
                if not self._is_shared_node_prop("scale"):
                    scale = None
                if not self._is_shared_node_prop("z_index"):
                    z_index = None

            # Only add a section if the node actually has transform-like properties
            if position is None and rotation is None and scale is None and z_index is None:
                return

            card = InspectorCard("Transform", eyebrow="Spatial")
            layout = card.body_layout

            # Position: Vec2 (2D) or Vec3 (3D)
            if isinstance(position, dict) and "x" in position and "y" in position:
                if "z" in position:
                    widget = Vec3PropertyWidget("Position")
                    widget.set_value(
                        (
                            float(position.get("x", 0.0)),
                            float(position.get("y", 0.0)),
                            float(position.get("z", 0.0)),
                        )
                    )
                else:
                    widget = Vec2PropertyWidget("Position")
                    widget.set_value(
                        (
                            float(position.get("x", 0.0)),
                            float(position.get("y", 0.0)),
                        )
                    )

                widget.value_changed.connect(
                    lambda v, name="position": self._on_node_property_changed(name, v)
                )
                self._mark_widget_mixed(widget, self._node_prop_is_mixed("position"))
                layout.addWidget(widget)

            # Rotation:
            # - 2D nodes expose a simple float rotation
            # - 3D nodes expose a quaternion rotation as {x, y, z, w}
            if rotation is not None:
                if isinstance(rotation, dict):
                    # Assume quaternion (Node3D/Camera3D) - use Euler widget
                    if all(k in rotation for k in ("x", "y", "z", "w")):
                        # Default quaternion is identity: (0, 0, 0, 1)
                        widget = EulerRotationWidget("Rotation", {"x": 0.0, "y": 0.0, "z": 0.0, "w": 1.0})
                        widget.set_value(rotation)
                        self._track_connection(
                            widget.value_changed,
                            lambda v, name="rotation": self._on_node_property_changed(name, v)
                        )
                        self._mark_widget_mixed(widget, self._node_prop_is_mixed("rotation"))
                        layout.addWidget(widget)
                else:
                    widget = FloatPropertyWidget("Rotation", 0.0)
                    widget.set_value(float(rotation))
                    widget.value_changed.connect(
                        lambda v, name="rotation": self._on_node_property_changed(name, v)
                    )
                    self._mark_widget_mixed(widget, self._node_prop_is_mixed("rotation"))
                    layout.addWidget(widget)

            # Scale: Vec2 (2D) or Vec3 (3D)
            if isinstance(scale, dict) and "x" in scale and "y" in scale:
                if "z" in scale:
                    widget = Vec3PropertyWidget("Scale", default_value=(1.0, 1.0, 1.0))
                    widget.set_value(
                        (
                            float(scale.get("x", 1.0)),
                            float(scale.get("y", 1.0)),
                            float(scale.get("z", 1.0)),
                        )
                    )
                else:
                    widget = Vec2PropertyWidget("Scale", default_value=(1.0, 1.0))
                    widget.set_value(
                        (
                            float(scale.get("x", 1.0)),
                            float(scale.get("y", 1.0)),
                        )
                    )

                self._track_connection(
                    widget.value_changed,
                    lambda v, name="scale": self._on_node_property_changed(name, v)
                )
                self._mark_widget_mixed(widget, self._node_prop_is_mixed("scale"))
                layout.addWidget(widget)

            # Z index for 2D nodes
            if z_index is not None:
                widget = IntPropertyWidget("Z Index")
                widget.set_value(int(z_index))
                self._track_connection(
                    widget.value_changed,
                    lambda v, name="z_index": self._on_node_property_changed(name, v)
                )
                self._mark_widget_mixed(widget, self._node_prop_is_mixed("z_index"))
                layout.addWidget(widget)

            if layout.count() == 0:
                # No usable transform values
                return

            self.properties_layout.addWidget(card)
        except Exception as e:
            t = inspector_tokens()
            error_label = QLabel(f"Error loading transform: {str(e)}")
            error_label.setWordWrap(True)
            error_label.setStyleSheet(f"color: {t.error};")
            self.properties_layout.addWidget(error_label)

    def _add_node_type_properties_section(self):
        """Render a node's own registered properties that aren't part of the base
        (name/active/visible) or transform (position/rotation/scale/z_index) sections.

        Camera2D (zoom, ortho_size, aspect_ratio, offset, follow/drag/limit, ...),
        CameraUI (canvas_size, origin, zoom, scale_factor, ...) and other nodes with
        custom node-level properties expose them here; previously only transform-like
        properties were shown, so e.g. a camera's zoom could not be edited at all.
        """
        if not self.current_node or not self.editor_bridge:
            return

        try:
            data = self._get_node_properties_data()

            properties = data.get("properties", {})
            if not isinstance(properties, dict) or not properties:
                return

            metadata = data.get("property_metadata", {})
            if not isinstance(metadata, dict):
                metadata = {}

            # Skip properties already presented by the Node base section
            # (name/active/visible) and the Transform section (position/rotation/scale/
            # z_index) so nothing renders twice.
            handled = {"name", "active", "visible", "position", "rotation", "scale", "z_index"}
            extra = {k: v for k, v in properties.items() if k not in handled}
            # In multi-selection mode keep only the properties common to every node.
            if self._multi_mode:
                extra = {k: v for k, v in extra.items() if self._is_shared_node_prop(k)}
            if not extra:
                return

            type_name = data.get("type") or "Node"
            if self._multi_mode:
                type_name = f"{type_name} (shared)"
            card = InspectorCard(type_name, eyebrow="Properties")

            for prop_name, prop_value in order_properties_by_declaration(extra, metadata):
                prop_metadata = metadata.get(prop_name)
                widget = self._create_property_widget(prop_name, prop_value, prop_metadata)
                if widget is None:
                    continue
                if hasattr(widget, "value_changed"):
                    self._track_connection(
                        widget.value_changed,
                        lambda v, name=prop_name: self._on_node_property_changed(name, v)
                    )
                self._mark_widget_mixed(widget, self._node_prop_is_mixed(prop_name))
                card.add_body_widget(widget)

            self.properties_layout.addWidget(card)
        except Exception as e:
            t = inspector_tokens()
            error_label = QLabel(f"Error loading node properties: {str(e)}")
            error_label.setWordWrap(True)
            error_label.setStyleSheet(f"color: {t.error};")
            self.properties_layout.addWidget(error_label)

    def _add_components_section(self):
        """Add components section"""
        # Multi-selection: render only the component groups shared across the whole
        # selection (matched by exact type or shared ancestor). Adding a component
        # to a heterogeneous selection has no single target, so that affordance is
        # intentionally omitted here.
        if self._multi_mode:
            for group in self._shared_component_groups:
                self._add_component_group(group["primary"], multi_ctx=group)
            return

        # Get all components
        components = self.current_node.get_components()

        for component in components:
            self._add_component_group(component)

        # Add Component: the inspector's primary action - a full-width dashed accent CTA
        t = inspector_tokens()
        add_comp_layout = QHBoxLayout()
        add_comp_layout.setContentsMargins(0, 4, 0, 0)
        add_comp_btn = QPushButton("+  Add Component")
        add_comp_btn.setCursor(Qt.CursorShape.PointingHandCursor)
        add_comp_btn.setMinimumHeight(34)
        add_comp_btn.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)
        add_comp_btn.setStyleSheet(f"""
            QPushButton {{
                background-color: {with_alpha(t.accent, 0.10)};
                color: {t.accent_hover};
                border: 1px dashed {with_alpha(t.accent, 0.6)};
                border-radius: {t.card_radius}px;
                font-weight: 600;
                letter-spacing: 0.5px;
                padding: 6px 14px;
            }}
            QPushButton:hover {{
                background-color: {with_alpha(t.accent, 0.20)};
                border: 1px solid {t.accent};
                color: {t.text_on_accent};
            }}
            QPushButton:pressed {{
                background-color: {with_alpha(t.accent, 0.30)};
            }}
        """)
        add_comp_btn.clicked.connect(self._on_add_component_clicked)
        add_comp_layout.addWidget(add_comp_btn)
        self.properties_layout.addLayout(add_comp_layout)

    def _add_component_group(self, component, multi_ctx=None):
        """Add a component group with its properties.

        ``multi_ctx`` is supplied in shared multi-node editing: it carries the
        matched peer components, the title to display (the matched type, which may
        be a shared ancestor rather than the component's own type), the set of
        properties common to all peers (``allowed``) and those whose values differ
        (``mixed``). Edits broadcast to every peer through ``_component_targets``.
        """
        multi = multi_ctx is not None
        t = inspector_tokens()

        # Custom components report their inheritance chain (most-derived first).
        # Surface the immediate base in the eyebrow and the full ancestry as a
        # tooltip. Guarded so an older runtime without get_type_chain still works.
        type_chain = []
        try:
            type_chain = list(component.get_type_chain())
        except Exception:
            type_chain = []

        if multi:
            eyebrow = f"Shared · {len(self.selected_nodes)} nodes"
            card_title = multi_ctx["title"]
        else:
            eyebrow = "Component"
            if len(type_chain) > 1:
                eyebrow = f"Component · extends {type_chain[1]}"
            card_title = component.get_type_name()

        card = InspectorCard(card_title, eyebrow=eyebrow)
        if len(type_chain) > 1:
            card.setToolTip("Inheritance: " + " → ".join(type_chain))
        group = card
        group_layout = card.body_layout

        # Restore the collapse state this card had before the last refresh so
        # adding/removing a component doesn't re-expand everything. Keyed by
        # (type_name, occurrence_index) to disambiguate duplicate components.
        type_name = component.get_type_name()
        occurrence = self._card_type_counter.get(type_name, 0)
        self._card_type_counter[type_name] = occurrence + 1
        card_state_key = (type_name, occurrence)
        if card_state_key in self._component_card_expanded:
            card.set_expanded(self._component_card_expanded[card_state_key])
        self._track_connection(
            card.toggled,
            lambda expanded, k=card_state_key: self._component_card_expanded.__setitem__(k, expanded))

        # Enable toggle lives in the header (replaces the old full-width "Enabled" row)
        enabled_checkbox = QCheckBox()
        enabled_checkbox.setToolTip("Component enabled")
        enabled_checkbox.blockSignals(True)
        enabled_checkbox.setChecked(component.is_enabled())
        enabled_checkbox.blockSignals(False)
        if multi:
            self._track_connection(
                enabled_checkbox.toggled,
                lambda v, peers=multi_ctx["peers"]: self._set_components_enabled_multi(peers, v))
        else:
            self._track_connection(
                enabled_checkbox.toggled, lambda v, c=component: c.set_enabled(v))
        card.add_header_widget(enabled_checkbox)

        # Duplicate affordance (adds an identical second component to this node).
        # Omitted in multi mode where there is no single target node.
        if not multi:
            duplicate_btn = QPushButton()
            duplicate_btn.setIcon(inspector_icon("duplicate", t.text_secondary))
            duplicate_btn.setIconSize(QSize(14, 14))
            duplicate_btn.setFixedSize(t.control_height, t.control_height)
            duplicate_btn.setToolTip(f"Duplicate {component.get_type_name()}")
            duplicate_btn.setCursor(Qt.CursorShape.PointingHandCursor)
            style_icon_button(duplicate_btn)
            duplicate_btn.clicked.connect(
                lambda checked=False, c=component: self._on_duplicate_component_clicked(c)
            )
            card.add_header_widget(duplicate_btn)

        # Compact remove affordance (danger-tinted on hover)
        remove_btn = QPushButton("✕")
        remove_btn.setFixedSize(t.control_height, t.control_height)
        remove_btn.setToolTip(f"Remove {component.get_type_name()}")
        remove_btn.setCursor(Qt.CursorShape.PointingHandCursor)
        style_icon_button(remove_btn, danger=True)
        if multi:
            remove_btn.setToolTip(f"Remove {card_title} from all selected nodes")
            remove_btn.clicked.connect(
                lambda checked=False, peers=multi_ctx["peers"]: self._remove_components_multi(peers)
            )
        else:
            remove_btn.clicked.connect(
                lambda checked=False, c=component: self._on_remove_component_clicked(c)
            )
        card.add_header_widget(remove_btn)

        # Get component properties from editor bridge
        if self.editor_bridge:
            try:
                properties_json = self.editor_bridge.get_component_properties(component)

                # Parse JSON string to dictionary
                try:
                    if isinstance(properties_json, str):
                        data = json.loads(properties_json)
                    else:
                        data = properties_json
                except json.JSONDecodeError as e:
                    if 'console' in self.main_editor.panels:
                        self.main_editor.panels['console'].log_message(
                            f"JSON Parse Error: {str(e)}\nRaw: {properties_json[:200]}", "Error")
                    raise

                # Extract properties and metadata
                properties = data.get("properties", {})
                metadata = data.get("property_metadata", {})

                # Filter out 'type' and 'enabled' properties as they're handled separately
                if "type" in properties:
                    del properties["type"]
                if "enabled" in properties:
                    del properties["enabled"]

                # Multi-selection: restrict to the properties common to every peer.
                if multi and isinstance(properties, dict):
                    allowed = multi_ctx.get("allowed")
                    if allowed is not None:
                        properties = {k: v for k, v in properties.items() if k in allowed}

                # Mixed-value set for marking rows whose value differs across peers.
                mixed_props = multi_ctx["mixed"] if multi else None

                # Handle case where properties might not be a dict
                if properties is None or len(properties) == 0:
                    info_label = QLabel("No editable properties")
                    info_label.setStyleSheet(
                        f"color: {t.text_disabled}; font-style: italic; padding: 2px 0;")
                    group_layout.addWidget(info_label)
                elif isinstance(properties, dict):
                    # Special handling for material widgets - store for later
                    material_widget = None
                    particle_gradient_widget = None
                    particle_curve_widget = None

                    # The per-component custom editors below (material slots, button
                    # states, curve/particle editors, ...) each target a single
                    # component, so they're skipped while multiple nodes are selected;
                    # the intersected simple properties are rendered generically and
                    # broadcast to every peer instead.
                    if multi:
                        pass
                    elif component.get_type_name() == "PrimitiveMesh3D":
                        # Create material override widget but don't add it yet
                        material_widget = MaterialOverridePropertyWidget("Material Override", component, self.editor_bridge)
                        material_widget.set_value(None)  # Will read from component

                        # Filter out material override properties from regular display
                        material_props = {
                            'materialOverrideEnabled', 'albedoColor', 'albedoTexture',
                            'metallic', 'roughness', 'metallicRoughnessTexture',
                            'normalTexture', 'normalScale', 'emissiveColor',
                            'emissiveTexture', 'emissiveStrength', 'shaderType',
                            'shadowBands', 'shadowThreshold', 'shadowSoftness',
                            'specularBands', 'specularPower', 'rimIntensity', 'rimPower'
                        }
                        properties = {k: v for k, v in properties.items() if k not in material_props}

                    elif component.get_type_name() == "StaticMesh3D":
                        # Create material slots widget for StaticMesh3D
                        from editor.widgets.material_slots_widget import MaterialSlotsPropertyWidget
                        material_widget = MaterialSlotsPropertyWidget(component, self.editor_bridge, self.main_editor)
                        # Store reference for later refresh
                        self._material_widgets[component] = material_widget
                        # No need to filter properties for StaticMesh3D - material slots are separate

                    elif component.get_type_name() in ("ColorRect", "Image2D", "Panel", "Shape2D"):
                        # Create the material-override / custom-shader widget. The shader lives in
                        # the materialOverride slot; Shape2D uses a shape-aware template, the
                        # rect-based components share the 2D UI template.
                        from editor.widgets.shader_attachment_widget import ShaderAttachmentWidget
                        template = ("Shape 2D" if component.get_type_name() == "Shape2D"
                                    else "Color Rect (2D UI)")
                        material_widget = ShaderAttachmentWidget(
                            component, self.editor_bridge, self.main_editor, default_template=template)
                        self._shader_attachment_widgets.append(material_widget)

                        # The material override and its serialized shader parameters are handled
                        # by the widget; hide the raw fields from the generic property list.
                        shader_attachment_props = {'materialOverride', 'shaderParameters'}
                        properties = {k: v for k, v in properties.items() if k not in shader_attachment_props}

                    elif component.get_type_name() == "SkeletalMesh3D":
                        # Create material slots widget for SkeletalMesh3D
                        from editor.widgets.material_slots_widget import MaterialSlotsPropertyWidget
                        material_widget = MaterialSlotsPropertyWidget(component, self.editor_bridge, self.main_editor)
                        # Store reference for later refresh
                        self._material_widgets[component] = material_widget
                        # No need to filter properties for SkeletalMesh3D - material slots are separate

                    elif component.get_type_name() == "CollisionBody2DComponent":
                        # Create collision body 2D widget but don't add it yet
                        from editor.widgets.collision_body_2d_widget import CollisionBody2DPropertyWidget
                        collision_body_widget = CollisionBody2DPropertyWidget(component, self.editor_bridge, self.main_editor)

                        # Connect polygon mode signal to viewport
                        if self.main_editor and hasattr(self.main_editor, 'viewport_tabs'):
                            viewport = self.main_editor.viewport_tabs.get_current_viewport()
                            if viewport:
                                collision_body_widget.polygon_mode_changed.connect(viewport.set_polygon_creation_mode)
                                # Connect viewport's vertex added signal to widget's refresh
                                viewport.polygon_vertex_added.connect(collision_body_widget._refresh_vertex_list)

                        # Filter out properties that are handled by the custom widget
                        collision_props = {'shapeType'}
                        properties = {k: v for k, v in properties.items() if k not in collision_props}

                    elif component.get_type_name() == "AnimationPlayer":
                        # Clip library editor + "Open Animation Timeline" launcher
                        from editor.widgets.animation_player_widget import AnimationPlayerPropertyWidget
                        anim_player_widget = AnimationPlayerPropertyWidget(component, self.editor_bridge, self.main_editor)

                    elif component.get_type_name() == "AnimationTree":
                        # Graph path + "Open Blend Tree" launcher
                        from editor.widgets.animation_tree_widget import AnimationTreePropertyWidget
                        anim_tree_widget = AnimationTreePropertyWidget(component, self.editor_bridge, self.main_editor)

                    elif component.get_type_name() == "Line2D":
                        # Create Line2D widget with Bezier curve support
                        from editor.widgets.line_2d_widget import Line2DPropertyWidget
                        line2d_widget = Line2DPropertyWidget(component, self.editor_bridge, self.main_editor)

                        # Connect line mode signal to viewport
                        if self.main_editor and hasattr(self.main_editor, 'viewport_tabs'):
                            viewport = self.main_editor.viewport_tabs.get_current_viewport()
                            if viewport:
                                line2d_widget.line_mode_changed.connect(viewport.set_line_creation_mode)
                                # Connect bezier edit signal to viewport
                                if hasattr(viewport, 'set_bezier_edit_component'):
                                    line2d_widget.bezier_edit_changed.connect(viewport.set_bezier_edit_component)
                                # Connect viewport's point added signal to widget's refresh
                                if hasattr(viewport, 'line_point_added'):
                                    viewport.line_point_added.connect(line2d_widget._refresh_points_list)

                        # Filter out properties that are handled by the custom widget
                        line2d_props = {'pointsData', 'bezierSegments'}
                        properties = {k: v for k, v in properties.items() if k not in line2d_props}

                    elif component.get_type_name() in ("Curve2D", "Path2D"):
                        # Create Curve2D/Path2D widget with Bezier curve support
                        from editor.widgets.curve_2d_widget import Curve2DPropertyWidget
                        curve2d_widget = Curve2DPropertyWidget(component, self.editor_bridge, self.main_editor)

                        # Connect curve mode signal to viewport
                        if self.main_editor and hasattr(self.main_editor, 'viewport_tabs'):
                            viewport = self.main_editor.viewport_tabs.get_current_viewport()
                            if viewport:
                                # Reuse line creation mode for curves
                                curve2d_widget.curve_mode_changed.connect(viewport.set_line_creation_mode)
                                if hasattr(viewport, 'set_bezier_edit_component'):
                                    curve2d_widget.bezier_edit_changed.connect(viewport.set_bezier_edit_component)
                                if hasattr(viewport, 'line_point_added'):
                                    viewport.line_point_added.connect(curve2d_widget._refresh_points_list)

                        # Filter out properties that are handled by the custom widget
                        curve2d_props = {'pointsData', 'bezierSegments'}
                        properties = {k: v for k, v in properties.items() if k not in curve2d_props}

                    elif component.get_type_name() in ("Curve3D", "Path3D"):
                        # Create Curve3D/Path3D widget with 3D Bezier + tilt editing
                        from editor.widgets.curve_3d_widget import Curve3DPropertyWidget
                        curve3d_widget = Curve3DPropertyWidget(component, self.editor_bridge, self.main_editor)

                        if self.main_editor and hasattr(self.main_editor, 'viewport_tabs'):
                            viewport = self.main_editor.viewport_tabs.get_current_viewport()
                            if viewport:
                                if hasattr(viewport, 'set_curve3d_creation_mode'):
                                    curve3d_widget.curve3d_mode_changed.connect(viewport.set_curve3d_creation_mode)
                                if hasattr(viewport, 'set_curve3d_edit_component'):
                                    curve3d_widget.curve3d_edit_changed.connect(viewport.set_curve3d_edit_component)
                                if hasattr(viewport, 'curve3d_point_added'):
                                    viewport.curve3d_point_added.connect(curve3d_widget._refresh_points_list)

                        # Filter out properties that are handled by the custom widget
                        curve3d_props = {'pointsData', 'bezierSegments'}
                        properties = {k: v for k, v in properties.items() if k not in curve3d_props}

                    elif component.get_type_name() == "NavigationRegion2D":
                        # Interactive outline drawing, reusing the polygon click-to-draw frontend.
                        from editor.widgets.polygon_property_widget import PolygonPropertyWidget
                        nav_region_widget = PolygonPropertyWidget(
                            component, self.editor_bridge, self.main_editor,
                            property_name="outline", title="Nav Outline")

                        if self.main_editor and hasattr(self.main_editor, 'viewport_tabs'):
                            viewport = self.main_editor.viewport_tabs.get_current_viewport()
                            if viewport:
                                nav_region_widget.polygon_mode_changed.connect(
                                    lambda enabled, vp=viewport: vp.set_polygon_property_mode(
                                        enabled, "NavigationRegion2D", "outline"))
                                viewport.polygon_vertex_added.connect(nav_region_widget._refresh_vertex_list)

                        # The raw outline array is handled by the custom widget.
                        properties = {k: v for k, v in properties.items() if k != 'outline'}

                    elif component.get_type_name() == "NavigationObstacle2D":
                        # Interactive carve-polygon drawing, reusing the same frontend.
                        from editor.widgets.polygon_property_widget import PolygonPropertyWidget
                        nav_obstacle_widget = PolygonPropertyWidget(
                            component, self.editor_bridge, self.main_editor,
                            property_name="vertices", title="Carve Polygon")

                        if self.main_editor and hasattr(self.main_editor, 'viewport_tabs'):
                            viewport = self.main_editor.viewport_tabs.get_current_viewport()
                            if viewport:
                                nav_obstacle_widget.polygon_mode_changed.connect(
                                    lambda enabled, vp=viewport: vp.set_polygon_property_mode(
                                        enabled, "NavigationObstacle2D", "vertices"))
                                viewport.polygon_vertex_added.connect(nav_obstacle_widget._refresh_vertex_list)

                        # The raw vertices array is handled by the custom widget.
                        properties = {k: v for k, v in properties.items() if k != 'vertices'}

                    elif component.get_type_name() in ("Particles2D", "Particles3D"):
                        # Color-over-life is edited as a multi-stop gradient strip and
                        # scale-over-life as a curve. Hide the raw color rows and the
                        # JSON gradient/curve strings from the generic property list.
                        from editor.widgets.particle_gradient_widget import ParticleGradientWidget
                        from editor.widgets.particle_curve_widget import ParticleCurveWidget
                        particle_gradient_widget = ParticleGradientWidget(
                            component, self.editor_bridge, self.main_editor)
                        particle_curve_widget = ParticleCurveWidget(
                            component, self.editor_bridge, self.main_editor)
                        particle_hidden_props = {'colorStart', 'colorEnd', 'colorGradient', 'scaleCurve'}
                        properties = {k: v for k, v in properties.items() if k not in particle_hidden_props}

                    elif component.get_type_name() == "CollisionMesh3DComponent":
                        # Create collision mesh 3D widget
                        from editor.widgets.collision_mesh_3d_widget import CollisionMesh3DPropertyWidget
                        collision_mesh_widget = CollisionMesh3DPropertyWidget(component, self.editor_bridge, self.main_editor)

                        # Filter out properties that are handled by the custom widget
                        collision_mesh_props = {'shapeType', 'size', 'radius', 'height', 'planeNormal', 'planeDistance', 'meshPath', 'meshSolver'}
                        properties = {k: v for k, v in properties.items() if k not in collision_mesh_props}

                    elif component.get_type_name() == "Button":
                        # Create button state widget
                        button_state_widget = ButtonStatePropertyWidget("Button States", component, self.editor_bridge)
                        button_state_widget.set_value(None)  # Will read from component

                        # Filter out button state properties from regular display
                        button_state_props = {
                            'styleMode', 'normalModulation', 'hoverModulation', 'pressedModulation', 'disabledModulation',
                            'normalSoundPath', 'hoverSoundPath', 'pressedSoundPath', 'disabledSoundPath',
                            'normalTweenEnabled', 'normalTweenScale', 'normalTweenRotation', 'normalTweenPosition', 'normalTweenDuration',
                            'hoverTweenEnabled', 'hoverTweenScale', 'hoverTweenRotation', 'hoverTweenPosition', 'hoverTweenDuration',
                            'pressedTweenEnabled', 'pressedTweenScale', 'pressedTweenRotation', 'pressedTweenPosition', 'pressedTweenDuration',
                            'disabledTweenEnabled', 'disabledTweenScale', 'disabledTweenRotation', 'disabledTweenPosition', 'disabledTweenDuration'
                        }
                        properties = {k: v for k, v in properties.items() if k not in button_state_props}

                    elif component.get_type_name() == "WorldEnvironment":
                        # Create custom WorldEnvironment widget
                        world_env_widget = WorldEnvironmentPropertyWidget("World Environment", component, self.editor_bridge)
                        world_env_widget.set_value(None)  # Will read from component

                        # Filter out all properties since they're handled by the custom widget
                        world_env_props = {
                            'skyboxType', 'skyboxColor', 'skyTopColor', 'skyHorizonColor', 'skyBottomColor',
                            'cubemapPosX', 'cubemapNegX', 'cubemapPosY', 'cubemapNegY', 'cubemapPosZ', 'cubemapNegZ',
                            'panoramicTexture', 'fogEnabled', 'fogColor', 'fogDensity', 'fogStart', 'fogEnd', 'fogMode',
                            'ambientLightEnabled', 'ambientLightColor', 'ambientLightIntensity', 'volumetricFogEnabled'
                        }
                        properties = {k: v for k, v in properties.items() if k not in world_env_props}

                    elif component.get_type_name() in ("VideoPlayer", "GifPlayer"):
                        # Live preview controls (Replay / Play / Pause, plus a seek
                        # scrubber for video). All properties stay shown normally;
                        # this only appends a control bar at the bottom.
                        media_control_widget = MediaPreviewControlWidget(
                            component, self.editor_bridge,
                            show_seek=(component.get_type_name() == "VideoPlayer"))

                    # Organize properties by group
                    grouped_properties = {}  # {group_name: [(prop_name, prop_value, prop_metadata), ...]}
                    ungrouped_properties = []  # [(prop_name, prop_value, prop_metadata), ...]
                    priority_properties = []  # [(prop_name, prop_value, prop_metadata), ...] - displayed first

                    # Check if this is a script component - need to show name and script_path first
                    is_script_component = component.get_type_name() in ('LuaScriptComponent', 'PythonScriptComponent', 'MRubyScriptComponent')
                    script_priority_props = ['name', 'script_path'] if is_script_component else []

                    for prop_name, prop_value in order_properties_by_declaration(properties, metadata):
                        # Skip nested properties dict (handle it separately if needed)
                        if prop_name == "properties" and isinstance(prop_value, dict):
                            # Unfold nested properties
                            for nested_name, nested_value in prop_value.items():
                                nested_meta = metadata.get("properties", {}).get(nested_name) if "properties" in metadata else None
                                # Check for group in nested metadata
                                group_name = nested_meta.get("group", "") if nested_meta else ""
                                if group_name:
                                    if group_name not in grouped_properties:
                                        grouped_properties[group_name] = []
                                    grouped_properties[group_name].append((nested_name, nested_value, nested_meta))
                                else:
                                    ungrouped_properties.append((nested_name, nested_value, nested_meta))
                        else:
                            # Get metadata for this property
                            prop_metadata = metadata.get(prop_name)
                            # Check for group in metadata
                            group_name = prop_metadata.get("group", "") if prop_metadata else ""

                            # For script components, priority properties go first
                            if prop_name in script_priority_props:
                                priority_properties.append((prop_name, prop_value, prop_metadata))
                            elif group_name:
                                if group_name not in grouped_properties:
                                    grouped_properties[group_name] = []
                                grouped_properties[group_name].append((prop_name, prop_value, prop_metadata))
                            else:
                                ungrouped_properties.append((prop_name, prop_value, prop_metadata))

                    # Sort priority properties so 'name' comes before 'script_path'
                    priority_properties.sort(key=lambda x: script_priority_props.index(x[0]) if x[0] in script_priority_props else 999)

                    # Add priority properties first (e.g., name and script_path for script components)
                    for prop_name, prop_value, prop_metadata in priority_properties:
                        self._add_property_row(group_layout.addWidget, prop_name, prop_value,
                                               prop_metadata, properties, component, mixed_props)

                    # Add ungrouped properties next
                    for prop_name, prop_value, prop_metadata in ungrouped_properties:
                        self._add_property_row(group_layout.addWidget, prop_name, prop_value,
                                               prop_metadata, properties, component, mixed_props)

                    # Add grouped properties with collapsible group boxes. For
                    # UIControl-derived components the shared layout/anchor groups are
                    # ordered after the component's own groups so individual properties
                    # (e.g. a Label's text) are shown before the alignment controls.
                    is_uicontrol = 'anchorPreset' in properties and 'layoutMode' in properties
                    for group_name in order_component_groups(grouped_properties.keys(), is_uicontrol):
                        # Create collapsible group box
                        collapsible_group = CollapsibleGroupBox(group_name)

                        # Add properties in this group (enable toggle first)
                        for prop_name, prop_value, prop_metadata in order_group_properties(grouped_properties[group_name]):
                            self._add_property_row(collapsible_group.add_widget, prop_name, prop_value,
                                                   prop_metadata, properties, component, mixed_props)

                        group_layout.addWidget(collapsible_group)

                    # Layout diagnostics for UI controls: the resolved rect, the computed
                    # minimum, and which property is actually driving each axis. When a
                    # control lands somewhere unexpected, this is the only way to see why.
                    if is_uicontrol and hasattr(component, 'get_axis_driver'):
                        try:
                            from editor.widgets.anchor_pips_widget import LayoutDiagnosticsWidget
                        except ImportError:
                            from widgets.anchor_pips_widget import LayoutDiagnosticsWidget
                        diagnostics_group = CollapsibleGroupBox("Layout Diagnostics")
                        diagnostics_group.add_widget(LayoutDiagnosticsWidget(component))
                        group_layout.addWidget(diagnostics_group)
                    
                    # Add material override widget at the bottom if it exists
                    if material_widget is not None:
                        group_layout.addWidget(material_widget)

                    # Add collision body 2D widget at the bottom if it exists
                    if 'collision_body_widget' in locals() and collision_body_widget is not None:
                        group_layout.addWidget(collision_body_widget)

                    # Add Line2D widget at the bottom if it exists
                    if 'line2d_widget' in locals() and line2d_widget is not None:
                        group_layout.addWidget(line2d_widget)

                    # Add Curve2D/Path2D widget at the bottom if it exists
                    if 'curve2d_widget' in locals() and curve2d_widget is not None:
                        group_layout.addWidget(curve2d_widget)

                    # Add Curve3D/Path3D widget at the bottom if it exists
                    if 'curve3d_widget' in locals() and curve3d_widget is not None:
                        group_layout.addWidget(curve3d_widget)

                    # Add navigation polygon widgets at the bottom if they exist
                    if 'nav_region_widget' in locals() and nav_region_widget is not None:
                        group_layout.addWidget(nav_region_widget)
                    if 'nav_obstacle_widget' in locals() and nav_obstacle_widget is not None:
                        group_layout.addWidget(nav_obstacle_widget)

                    # Add particle color-over-life gradient + scale-over-life curve widgets
                    if 'particle_gradient_widget' in locals() and particle_gradient_widget is not None:
                        group_layout.addWidget(particle_gradient_widget)
                    if 'particle_curve_widget' in locals() and particle_curve_widget is not None:
                        group_layout.addWidget(particle_curve_widget)

                    # Add animation player/tree widgets at the bottom if they exist
                    if 'anim_player_widget' in locals() and anim_player_widget is not None:
                        group_layout.addWidget(anim_player_widget)
                    if 'anim_tree_widget' in locals() and anim_tree_widget is not None:
                        group_layout.addWidget(anim_tree_widget)

                    # Add collision mesh 3D widget at the bottom if it exists
                    if 'collision_mesh_widget' in locals() and collision_mesh_widget is not None:
                        group_layout.addWidget(collision_mesh_widget)

                    # Add button state widget at the bottom if it exists
                    if 'button_state_widget' in locals() and button_state_widget is not None:
                        group_layout.addWidget(button_state_widget)

                    # Add world environment widget at the bottom if it exists
                    if 'world_env_widget' in locals() and world_env_widget is not None:
                        group_layout.addWidget(world_env_widget)

                    # Add media preview controls (VideoPlayer / GifPlayer) at the bottom
                    if 'media_control_widget' in locals() and media_control_widget is not None:
                        group_layout.addWidget(media_control_widget)
                else:
                    error_label = QLabel(f"Unexpected properties format: {type(properties).__name__}")
                    error_label.setWordWrap(True)
                    error_label.setStyleSheet(f"color: {t.error};")
                    group_layout.addWidget(error_label)
            except Exception as e:
                import traceback
                error_msg = f"Error loading properties: {str(e)}\n{traceback.format_exc()}"
                if 'console' in self.main_editor.panels:
                    self.main_editor.panels['console'].log_message(error_msg, "Error")
                error_label = QLabel(f"Error loading properties: {str(e)}")
                error_label.setWordWrap(True)
                error_label.setStyleSheet(f"color: {t.error};")
                group_layout.addWidget(error_label)

        self.properties_layout.addWidget(group)

    def _title_case_property_name(self, prop_name):
        """Convert property name to title case for display"""
        # Use the global format_property_name function
        return format_property_name(prop_name)

    def _ensure_custom_widgets_discovered(self):
        """Lazily import built-in and project-addon custom inspector widgets.

        Done on first use (not at import time) so the addon modules can safely import
        PropertyWidget from this module without a circular import.
        """
        if getattr(self, '_custom_widgets_discovered', False):
            return
        self._custom_widgets_discovered = True
        try:
            custom_widget_registry.discover_builtin_widgets()
        except Exception:
            pass
        project_dir = None
        try:
            project = getattr(self.main_editor, 'project', None) if self.main_editor else None
            if project is not None:
                for attr in ('project_dir', 'directory', 'root_dir', 'path', 'project_path'):
                    candidate = getattr(project, attr, None)
                    if candidate:
                        project_dir = os.path.dirname(candidate) if str(candidate).endswith(('.lupine', '.json')) else str(candidate)
                        break
        except Exception:
            project_dir = None
        if project_dir:
            try:
                custom_widget_registry.discover_project_widgets(project_dir)
            except Exception:
                pass

    def _usage_flags(self, prop_metadata):
        """Read the PropertyUsageFlags bitmask from metadata (0 when absent)."""
        if not prop_metadata:
            return 0
        try:
            return int(prop_metadata.get("usage", 0) or 0)
        except (ValueError, TypeError):
            return 0

    def _make_header_label(self, text):
        """A section header rendered above a property row ([Header(...)] attribute)."""
        t = inspector_tokens()
        label = QLabel(str(text))
        label.setStyleSheet(
            f"color: {t.text_secondary}; font-weight: 600; padding: 6px 2px 2px 2px;")
        return label

    def _apply_suffix(self, widget, suffix):
        """Append a dim unit suffix label to a widget's primary horizontal row."""
        layout = widget.layout()
        if layout is None or not isinstance(layout, QHBoxLayout):
            return
        t = inspector_tokens()
        suffix_label = QLabel(str(suffix))
        suffix_label.setStyleSheet(f"color: {t.text_disabled}; padding-left: 2px;")
        insert_at = layout.count()
        if getattr(widget, 'reset_holder', None) is not None and insert_at > 0:
            insert_at -= 1
        layout.insertWidget(insert_at, suffix_label)

    def _add_property_row(self, add_fn, prop_name, prop_value, prop_metadata, properties,
                          component, mixed_props=None):
        """Build one property row and add it via ``add_fn``, honouring the extended
        editor metadata: HideInInspector (skip), Header (preceding label), ReadOnly
        (disabled control), Suffix (unit label), plus the existing linked-widget wiring.

        ``mixed_props`` (multi-node editing) names the properties whose value differs
        across the selection; matching rows are flagged so the user sees that several
        values are being collapsed into one editable control.
        """
        usage = self._usage_flags(prop_metadata)
        if usage & USAGE_HIDDEN:
            return

        header = prop_metadata.get("header") if prop_metadata else None
        if header:
            add_fn(self._make_header_label(header))

        # Expose the owning component (and its full property dict) to the widget factory so
        # component-aware editors -- the theme-variation picker, the anchor pips, which need
        # anchorMax as well as anchorMin -- can resolve their context.
        self._prop_row_component = component
        self._prop_row_properties = properties
        try:
            widget = self._create_property_widget(prop_name, prop_value, prop_metadata)
        finally:
            self._prop_row_component = None
            self._prop_row_properties = None
        if not widget:
            return

        if isinstance(widget, LinkedVec4PropertyWidget):
            linked_prop_name = f"{prop_name}Linked"
            if linked_prop_name in properties:
                widget.set_linked(properties[linked_prop_name])
        elif isinstance(widget, LinkedColorPropertyWidget):
            if "borderColorLinked" in properties:
                widget.set_linked(properties["borderColorLinked"])
            if all(name in properties for name in
                   ['borderColorLeft', 'borderColorRight', 'borderColorTop', 'borderColorBottom']):
                widget.set_value([
                    properties['borderColorLeft'],
                    properties['borderColorRight'],
                    properties['borderColorTop'],
                    properties['borderColorBottom'],
                ])

        if usage & USAGE_READONLY:
            widget.setEnabled(False)

        # Disable, and explain, any layout field that cannot do anything in this control's
        # current configuration. Without this the user edits a number, watches it revert, and
        # gets no explanation -- the most confusing failure mode in the whole layout system.
        inert_reason = self._layout_inert_reason(prop_name, properties, component)
        if inert_reason:
            widget.setEnabled(False)
            widget.setToolTip(inert_reason)

        suffix = prop_metadata.get("suffix") if prop_metadata else None
        if suffix:
            self._apply_suffix(widget, suffix)

        if mixed_props and prop_name in mixed_props:
            self._mark_widget_mixed(widget, True)

        self._track_connection(
            widget.value_changed,
            lambda v, c=component, p=prop_name: self._on_component_property_changed(c, p, v))
        add_fn(widget)

    # Driver codes returned by UIControl::GetAxisDriver.
    _AXIS_WIDTH_HEIGHT = 0
    _AXIS_OFFSETS = 1
    _AXIS_CONTAINER_DRIVEN = 2

    # The containers that actually read the inherited `separation` property.
    _SEPARATION_USERS = frozenset({
        'HorizontalContainer', 'VerticalContainer', 'ScrollContainer',
    })

    # ...and what to use instead, for the ones that do not.
    _SEPARATION_ALTERNATIVE = {
        'GridContainer': "Use Horizontal Spacing / Vertical Spacing.",
        'Wrap': "Use Spacing X / Spacing Y.",
        'DockContainer': "Use Dock Spacing.",
        'Stack': "A Stack overlays its children, so there is nothing to space apart.",
        'CenterContainer': "A CenterContainer centers a single child.",
        'PaddingContainer': "Use the Padding group.",
        'TabContainer': "A TabContainer shows one page at a time.",
    }

    def _layout_inert_reason(self, prop_name, properties, component):
        """Why `prop_name` has no effect on `component` right now, or None if it works.

        Roughly half of a UIControl's Layout group is dead in any given configuration, and
        which half depends on the layout mode, the anchors, and whether a parent container
        owns the rect. All three facts come from the engine (UIControl::GetAxisDriver), so
        this cannot drift away from what the solver actually reads.
        """
        if prop_name not in (
            'width', 'height', 'offsetMin', 'offsetMax', 'anchorMin', 'anchorMax',
            'anchorPreset', 'layoutMode', 'growDirectionH', 'growDirectionV',
            'sizeFlagsHorizontal', 'sizeFlagsVertical', 'sizeFlagsStretchRatio',
            'separation',
        ):
            return None

        # Only UIControl-derived components have any of this.
        if not hasattr(component, 'get_axis_driver'):
            return None

        # `separation` is defined on the Container base but only READ by the containers that
        # stack their children along an axis. The others each have their own spacing
        # properties, and the inherited slider sat there in the inspector doing nothing.
        if prop_name == 'separation':
            try:
                type_name = component.get_type_name()
            except Exception:
                return None
            if type_name in self._SEPARATION_USERS:
                return None
            return ("Not used by this container. "
                    + self._SEPARATION_ALTERNATIVE.get(
                        type_name, "It has no axis to space children along."))

        try:
            container_driven = component.is_container_driven()
            driver_h = component.get_axis_driver(False)
            driver_v = component.get_axis_driver(True)
        except Exception:
            return None

        in_container = bool(container_driven)

        # A parent container writes this control's rect every layout pass, so nothing that
        # feeds the anchor solver survives -- but the size FLAGS are how the child talks to
        # that container, and they are the one thing that does matter here.
        if in_container and prop_name not in (
                'sizeFlagsHorizontal', 'sizeFlagsVertical', 'sizeFlagsStretchRatio'):
            return ("Overridden by the parent container, which positions and sizes this "
                    "control. Use the size flags to tell the container what you want.")

        # Conversely, the size flags are only ever read BY a container.
        if not in_container and prop_name in (
                'sizeFlagsHorizontal', 'sizeFlagsVertical', 'sizeFlagsStretchRatio'):
            return ("Only read by a parent container. This control is not inside one, so its "
                    "size flags do nothing.")

        layout_mode = properties.get('layoutMode', 0)
        position_mode = (layout_mode == 0)

        if prop_name in ('offsetMin', 'offsetMax', 'anchorMin', 'anchorMax', 'anchorPreset'):
            if position_mode:
                return ("Ignored in Position layout mode, where the node's own position and "
                        "width/height place the control. Set Layout Mode to Anchors to use "
                        "anchors and offsets.")
            return None

        if prop_name == 'width':
            if driver_h == self._AXIS_OFFSETS:
                return ("Ignored: this control is anchor-stretched horizontally, so its width "
                        "is the distance between its left and right offsets. Edit those "
                        "instead, or set both horizontal anchors to the same value.")
            return None

        if prop_name == 'height':
            if driver_v == self._AXIS_OFFSETS:
                return ("Ignored: this control is anchor-stretched vertically, so its height "
                        "is the distance between its top and bottom offsets. Edit those "
                        "instead, or set both vertical anchors to the same value.")
            return None

        # Grow direction only does something when the resolved size actually hits a limit.
        if prop_name in ('growDirectionH', 'growDirectionV'):
            axis_min, axis_max = ('customMinSize', 'customMaxSize')
            has_limit = False
            for limit_name in (axis_min, axis_max):
                limit = properties.get(limit_name)
                if isinstance(limit, dict):
                    component_value = limit.get('x' if prop_name.endswith('H') else 'y', 0)
                elif isinstance(limit, (list, tuple)) and len(limit) >= 2:
                    component_value = limit[0 if prop_name.endswith('H') else 1]
                else:
                    component_value = 0
                if component_value:
                    has_limit = True
            if not has_limit:
                return ("No effect: grow direction only decides which edge stays put when the "
                        "resolved size is clamped, and this control has no custom min or max "
                        "size on this axis to clamp against.")
            return None

        return None

    def _project_directory(self):
        """Absolute project directory, or "" when no project is open."""
        project = getattr(self.main_editor, "project", None) if self.main_editor else None
        if project is None:
            return ""
        try:
            return project.get_directory() or ""
        except Exception:
            return getattr(project, "directory", "") or ""

    def _discover_theme_variations(self, base_types=None):
        """Collect theme type variations from the project's ``.uitheme`` assets.

        Returns an ordered, de-duplicated list of ``(variation_name, base_type)`` pairs.
        A type entry is a variation when it carries an ``extends`` field. When
        ``base_types`` is provided (the selected component's theme type and its
        ancestors) only variations whose base is in that set are returned; otherwise
        every discovered variation is returned so the picker is never empty.
        """
        project_dir = self._project_directory()
        if not project_dir:
            return []
        bases = {str(b) for b in (base_types or []) if b}
        seen = set()
        results = []
        try:
            theme_files = sorted(Path(project_dir).rglob("*.uitheme"))
        except Exception:
            theme_files = []
        for theme_file in theme_files:
            try:
                with open(theme_file, "r", encoding="utf-8") as fh:
                    data = json.load(fh)
            except Exception:
                continue
            if not isinstance(data, dict):
                continue
            types = data.get("types")
            if not isinstance(types, dict):
                continue
            for type_name, type_data in types.items():
                if not isinstance(type_data, dict):
                    continue
                base = type_data.get("extends")
                if not isinstance(base, str) or not base.strip():
                    continue
                base = base.strip()
                if bases and base not in bases:
                    continue
                if type_name in seen:
                    continue
                seen.add(type_name)
                results.append((str(type_name), base))
        return results

    @staticmethod
    def _parse_hint_extensions(hint_string):
        """Parse a File-hint string into a list of accepted extensions.

        Accepts the common authoring forms ``*.png,*.jpg``, ``.png .jpg`` or
        ``png;jpg``. Returns None when no extensions can be parsed (accept anything).
        """
        if not hint_string:
            return None
        tokens = re.split(r"[,;\s]+", hint_string.strip())
        extensions = []
        for token in tokens:
            token = token.strip().lstrip("*").lower()
            if not token:
                continue
            if not token.startswith("."):
                token = "." + token
            if token != ".":
                extensions.append(token)
        return extensions or None

    # Property-name fragments that mark a string field as referencing an image asset.
    _IMAGE_NAME_HINTS = (
        "texture", "sprite", "image", "icon", "albedo", "diffuse", "normalmap",
        "emissive", "atlas", "thumbnail", "splash", "cursor", "bitmap",
    )

    def _is_image_field(self, prop_name, hint_string):
        """Whether a string property should use the rich image resource widget."""
        if hint_string:
            extensions = self._parse_hint_extensions(hint_string)
            if extensions and all(e in asset_drag.IMAGE_EXTENSIONS for e in extensions):
                return True
        lowered = (prop_name or "").lower()
        return any(fragment in lowered for fragment in self._IMAGE_NAME_HINTS)

    def _create_property_widget(self, prop_name, prop_value, prop_metadata=None):
        """Create appropriate widget based on property type and metadata"""
        # Honour HideInInspector here too so any direct caller (e.g. the node-property
        # path) skips hidden properties, not just the component row builder.
        if prop_metadata and (self._usage_flags(prop_metadata) & USAGE_HIDDEN):
            return None

        # Use title cased name for display
        display_name = self._title_case_property_name(prop_name)

        # UIControl layout properties get friendlier custom editors. These props are
        # present only on UIControl-derived components, so the branches act as the
        # presence check. Returning the custom widget here means the existing group
        # placement and value_changed wiring apply automatically (no double-render).
        if prop_name == 'themeTypeVariation':
            base_types = []
            component = getattr(self, '_prop_row_component', None)
            if component is not None:
                try:
                    base_types = list(component.get_type_chain())
                except Exception:
                    try:
                        base_types = [component.get_type_name()]
                    except Exception:
                        base_types = []
            widget = ThemeVariationPropertyWidget(
                display_name, "", self, base_types)
            widget.set_value(prop_value if isinstance(prop_value, str) else "")
            return widget
        if prop_name == 'anchorPreset':
            try:
                from editor.widgets.anchor_preset_widget import AnchorPresetPropertyWidget
            except ImportError:
                from widgets.anchor_preset_widget import AnchorPresetPropertyWidget
            widget = AnchorPresetPropertyWidget(
                display_name, prop_value if isinstance(prop_value, int) else 0)
            widget.set_value(prop_value if prop_value is not None else 0)
            return widget
        if prop_name in ('sizeFlagsHorizontal', 'sizeFlagsVertical'):
            try:
                from editor.widgets.size_flags_widget import SizeFlagsPropertyWidget
            except ImportError:
                from widgets.size_flags_widget import SizeFlagsPropertyWidget
            widget = SizeFlagsPropertyWidget(
                display_name, prop_value if isinstance(prop_value, int) else 1)
            widget.set_value(prop_value if prop_value is not None else 1)
            return widget
        if prop_name == 'anchorMin':
            # Draggable pips instead of two raw Vec2 spinboxes. The widget owns anchorMax
            # too (an anchor is a rectangle, and editing one corner without the other is how
            # you accidentally invert it), so it writes that through sibling_changed.
            try:
                from editor.widgets.anchor_pips_widget import AnchorPipsPropertyWidget
            except ImportError:
                from widgets.anchor_pips_widget import AnchorPipsPropertyWidget
            widget = AnchorPipsPropertyWidget(display_name, prop_value)
            widget.set_value(prop_value)
            properties = getattr(self, '_prop_row_properties', None)
            if isinstance(properties, dict) and 'anchorMax' in properties:
                widget.set_anchor_max(properties['anchorMax'])
            component = getattr(self, '_prop_row_component', None)
            if component is not None:
                self._track_connection(
                    widget.sibling_changed,
                    lambda name, v, c=component: self._on_component_property_changed(c, name, v))
            return widget

        # Parse metadata if available
        prop_type = None
        hint_type = None
        hint_string = ""
        default_value = None

        if prop_metadata:
            try:
                prop_type = prop_metadata.get("type")  # PropertyValueType enum value
                # Ensure prop_type is an int if it exists
                if prop_type is not None and not isinstance(prop_type, int):
                    prop_type = int(prop_type)

                hint_info = prop_metadata.get("hint", {})
                if isinstance(hint_info, dict):
                    hint_type = hint_info.get("type")
                    if hint_type is not None and not isinstance(hint_type, int):
                        hint_type = int(hint_type)
                    hint_string = hint_info.get("hint_string", "")

                # Extract default value from metadata. C++ descriptors serialize the
                # key as "default"; older/editor paths used "defaultValue". Accept both.
                default_value = prop_metadata.get("defaultValue", prop_metadata.get("default"))
            except (ValueError, TypeError) as e:
                if 'console' in self.main_editor.panels:
                    self.main_editor.panels['console'].log_message(
                        f"Error parsing metadata for property '{prop_name}': {str(e)}", "Warning")

        # 1) Property-declared custom widget (frontend-agnostic id + JSON config).
        #    Resolved before any type-based dispatch so a property can fully override
        #    its editor. Falls through to the default widget if the id is unknown.
        if prop_metadata:
            custom_widget_id = prop_metadata.get("custom_widget")
            if custom_widget_id:
                self._ensure_custom_widgets_discovered()
                widget = custom_widget_registry.create_inspector_widget(
                    custom_widget_id, prop_name, prop_metadata, prop_value,
                    prop_metadata.get("custom_widget_config"))
                if widget is not None:
                    # Custom factories don't receive the editor bridge; inject it so
                    # widgets that need it (e.g. audio playback) work once built.
                    if getattr(widget, "editor_bridge", None) is None:
                        try:
                            widget.editor_bridge = self.editor_bridge
                        except Exception:
                            pass
                    return widget

            # 2) Inline struct (Dictionary backed by an object_schema) -> foldout editor.
            object_schema = prop_metadata.get("object_schema")
            if object_schema:
                return NestedStructPropertyWidget(
                    prop_name, object_schema,
                    prop_value if isinstance(prop_value, dict) else (default_value or {}),
                    self, prop_metadata.get("object_type", ""))

            # 3) Class-filtered reference pickers (NodeType / ScriptClass / ArchetypeType).
            if hint_type in (HINT_NODE_TYPE, HINT_SCRIPT_CLASS):
                return FilteredReferencePropertyWidget(
                    display_name, default_value if default_value is not None else "",
                    self, FilteredReferencePropertyWidget.KIND_NODE, hint_string)
            if hint_type == HINT_ARCHETYPE_TYPE:
                return FilteredReferencePropertyWidget(
                    display_name, default_value if default_value is not None else "",
                    self, FilteredReferencePropertyWidget.KIND_ARCHETYPE, hint_string)

        # PropertyValueType enum mapping (from PropertyDescriptor.hpp)
        # 0: Int, 1: Float, 2: String, 3: Bool, 4: Vec2, 5: Vec3, 6: Vec4, 7: Color, 8: NodePath, 9: ScenePath, 10: Enum

        # PropertyHintType enum mapping
        # 0: None, 1: Range, 2: Enum, 3: File, 4: MultilineText, 5: ExpRange, 6: Length, 7: ColorNoAlpha, 8: Dir, 9: Layers2D, 10: Layers3D

        # Check for linked Vec4 properties (cornerRadius, borderWidth, padding, margin)
        # Skip the *Linked properties themselves
        # Also check that the property type is actually Vec4 (type 6), not Float (type 1)
        if not prop_name.endswith('Linked') and prop_name in ['cornerRadius', 'borderWidth', 'padding', 'margin'] and prop_type == 6:
            # This is a linked property - use special widget
            # Convert default value if it's a dict
            default_val = (0, 0, 0, 0)  # Default fallback
            if default_value is not None:
                if isinstance(default_value, dict):
                    default_val = (default_value.get('x', 0), default_value.get('y', 0),
                                 default_value.get('z', 0), default_value.get('w', 0))
                elif hasattr(default_value, "__iter__") and len(default_value) >= 4:
                    default_val = default_value

            # Use appropriate labels based on property type
            # For padding/margin: Top, Right, Bottom, Left
            # For cornerRadius: TL, TR, BR, BL (Top-Left, Top-Right, etc.)
            # For borderWidth: Top, Right, Bottom, Left
            if prop_name in ['padding', 'margin', 'borderWidth']:
                labels = ['Top', 'Right', 'Bottom', 'Left']
            elif prop_name == 'cornerRadius':
                labels = ['TL', 'TR', 'BR', 'BL']
            else:
                labels = None  # Use default X, Y, Z, W

            widget = LinkedVec4PropertyWidget(display_name, f"{prop_name}Linked", default_val, labels)
            if isinstance(prop_value, dict):
                widget.set_value((
                    prop_value.get('x', 0),
                    prop_value.get('y', 0),
                    prop_value.get('z', 0),
                    prop_value.get('w', 0),
                ))
            elif hasattr(prop_value, "__iter__") and len(prop_value) >= 4:
                widget.set_value(prop_value)
            # Will set linked state from separate property later
            return widget
        elif prop_name == 'borderColorLeft':
            # This is the first border color property - create linked color widget for all 4
            # Check if we have all 4 border color properties
            # Note: component parameter needs to be passed from _add_component_group context
            # For now, try to get from prop_metadata or skip the check
            try:
                # We're being called from _add_component_group, so properties dict should have all props
                # Just create the widget - the properties will be set from the properties dict
                default_color = (0, 0, 0, 1) if default_value is None else default_value
                widget = LinkedColorPropertyWidget(
                    'Border Color',
                    color_names=['Left', 'Right', 'Top', 'Bottom'],
                    linked_prop_name='borderColorLinked',
                    default_value=default_color
                )
                # Value will be set by caller after checking all properties exist
                return widget
            except Exception:
                # Fall through to regular color widget if creation fails
                pass
        elif prop_name in ['borderColorRight', 'borderColorTop', 'borderColorBottom', 'borderColorLinked']:
            # Skip these - they're handled by the LinkedColorPropertyWidget for borderColorLeft
            return None
        elif prop_name.endswith('Linked'):
            # Skip rendering the *Linked bool properties - they're handled by LinkedVec4PropertyWidget
            return None

        # Extended property value types (Double=12, Quat=13, Rect=14, Resource=15,
        # IntArray=16, FloatArray=17, Array=18, Dictionary=19). Resolved up-front:
        # several share JSON shapes (dicts / lists) that the generic value inference
        # below would otherwise misclassify as vectors.
        if prop_type == 12:  # Double (edited like a float)
            widget = FloatPropertyWidget(display_name, default_value if default_value is not None else 0.0)
            widget.set_value(prop_value if prop_value is not None else 0.0)
            return widget
        if prop_type == 15:  # Resource (a res:// path to another asset)
            archetype_class = hint_string if hint_type == HINT_ARCHETYPE_TYPE else ""
            extensions = self._parse_hint_extensions(hint_string) if hint_type == 3 else None
            widget = ResourceFieldWidget(
                display_name, default_value if default_value is not None else "",
                editor_bridge=self.editor_bridge, extensions=extensions,
                archetype_class=archetype_class,
                audio_controls=("audio" in prop_name.lower()))
            widget.set_value(prop_value if prop_value is not None else "")
            return widget
        if prop_type == 13:  # Quat
            quat_default = default_value if isinstance(default_value, dict) else {"w": 1.0, "x": 0.0, "y": 0.0, "z": 0.0}
            widget = QuatPropertyWidget(display_name, quat_default)
            widget.set_value(prop_value if prop_value is not None else quat_default)
            return widget
        if prop_type == 14:  # Rect
            rect_default = default_value if isinstance(default_value, dict) else {"x": 0.0, "y": 0.0, "w": 0.0, "h": 0.0}
            widget = RectPropertyWidget(display_name, rect_default)
            widget.set_value(prop_value if prop_value is not None else rect_default)
            return widget
        _collection_kind = {11: "string_array", 16: "int_array", 17: "float_array",
                            18: "array", 19: "dict"}.get(prop_type)
        if _collection_kind is not None:
            widget = CollectionPropertyWidget(display_name, _collection_kind, default_value)
            widget.set_value(prop_value)
            return widget

        # Use metadata type hint if available, otherwise infer from value
        if prop_type == 3 or (prop_type is None and isinstance(prop_value, bool)):
            # Bool
            widget = BoolPropertyWidget(display_name, default_value if default_value is not None else False)
            widget.set_value(prop_value)
            return widget
        elif prop_type == 10 or hint_type == 2:  # Enum type or Enum hint
            # Create enum widget with hint string
            enum_values = [v.strip() for v in hint_string.split(',')] if hint_string else []
            widget = EnumPropertyWidget(display_name, enum_values, default_value if default_value is not None else 0)
            widget.set_value(prop_value)
            return widget
        elif hint_type == 9 or hint_type == 10:  # Layers2D (9) or Layers3D (10)
            # Collision layers widget
            from widgets.collision_layer_widget import CollisionLayerPropertyWidget

            # Get layer names from project if available
            layer_names = None
            if hasattr(self, 'main_editor') and self.main_editor and hasattr(self.main_editor, 'project'):
                project = self.main_editor.project
                if project:
                    if hint_type == 9:  # Layers2D
                        layer_names = getattr(project, 'collision_layers_2d', None)
                    else:  # Layers3D
                        layer_names = getattr(project, 'collision_layers_3d', None)

            is_3d = (hint_type == 10)
            widget = CollisionLayerPropertyWidget(
                display_name,
                default_value if default_value is not None else 1,
                is_3d=is_3d,
                layer_names=layer_names
            )
            widget.set_value(prop_value)
            return widget
        elif prop_type == 0 or (prop_type is None and isinstance(prop_value, int) and not isinstance(prop_value, bool)):
            # Int
            widget = IntPropertyWidget(display_name, default_value if default_value is not None else 0)
            widget.set_value(prop_value)
            return widget
        elif prop_type == 1 or (prop_type is None and isinstance(prop_value, float)):
            # Float
            widget = FloatPropertyWidget(display_name, default_value if default_value is not None else 0.0)
            widget.set_value(prop_value)
            return widget
        elif prop_type == 7 or (prop_type is None and "color" in prop_name.lower()):
            # Color
            # Convert default value if it's a dict
            default_val = None
            if default_value is not None:
                if isinstance(default_value, dict):
                    default_val = (default_value.get('r', 1), default_value.get('g', 1),
                                 default_value.get('b', 1), default_value.get('a', 1))
                elif hasattr(default_value, "__iter__") and len(default_value) >= 3:
                    default_val = default_value
            else:
                default_val = (1, 1, 1, 1)  # Default white
            widget = ColorPropertyWidget(display_name, default_val)
            if isinstance(prop_value, dict):
                # Color from dict format {r, g, b, a}
                widget.set_value((prop_value.get('r', 0), prop_value.get('g', 0),
                                prop_value.get('b', 0), prop_value.get('a', 1)))
            else:
                widget.set_value(prop_value)
            return widget
        elif prop_type == 2 or (prop_type is None and isinstance(prop_value, str)):
            # String - check for file/dir hints or path-related property names
            if hint_type == 3:  # File hint
                # Check if it's a tilemap file property
                if "tilemap" in prop_name.lower() or (hint_string and ".tilemap" in hint_string.lower()):
                    widget = TileMapPropertyWidget(display_name, self.main_editor)
                    widget.default_value = default_value if default_value is not None else ""
                    widget.set_value(prop_value)
                    return widget
                # Check if it's an audio file property
                elif "audio" in prop_name.lower():
                    widget = AudioFilePropertyWidget(display_name, self.editor_bridge)
                    widget.default_value = default_value if default_value is not None else ""
                    widget.set_value(prop_value)
                    return widget
                # Image file property -> rich resource field with inline preview
                elif self._is_image_field(prop_name, hint_string):
                    widget = ResourceFieldWidget(
                        display_name, default_value if default_value is not None else "",
                        editor_bridge=self.editor_bridge,
                        extensions=self._parse_hint_extensions(hint_string) or asset_drag.IMAGE_EXTENSIONS)
                    widget.set_value(prop_value)
                    return widget
                else:
                    widget = PathPropertyWidget(display_name, default_value if default_value is not None else "")
                    widget.set_value(prop_value)
                    return widget
            elif hint_type == 8:  # Dir hint
                widget = PathPropertyWidget(display_name, default_value if default_value is not None else "")
                widget.set_value(prop_value)
                return widget
            elif "audio" in prop_name.lower() and ("path" in prop_name.lower() or "file" in prop_name.lower() or "asset" in prop_name.lower()):
                # Audio file property detected by name
                widget = AudioFilePropertyWidget(display_name, self.editor_bridge)
                widget.default_value = default_value if default_value is not None else ""
                widget.set_value(prop_value)
                return widget
            elif self._is_image_field(prop_name, hint_string):
                # Fallback: detect image properties by name -> rich resource field
                widget = ResourceFieldWidget(
                    display_name, default_value if default_value is not None else "",
                    editor_bridge=self.editor_bridge, extensions=asset_drag.IMAGE_EXTENSIONS)
                widget.set_value(prop_value)
                return widget
            elif "path" in prop_name.lower() or "file" in prop_name.lower():
                # Fallback: detect path properties by name
                widget = PathPropertyWidget(display_name, default_value if default_value is not None else "")
                widget.set_value(prop_value)
                return widget
            else:
                widget = StringPropertyWidget(display_name, default_value if default_value is not None else "")
                widget.set_value(prop_value)
                return widget
        elif prop_type == 4 or (prop_type is None and isinstance(prop_value, dict) and 'x' in prop_value and 'y' in prop_value and 'z' not in prop_value):
            # Vec2
            # Convert default value if it's a dict
            default_val = None
            if default_value is not None:
                if isinstance(default_value, dict):
                    default_val = (default_value.get('x', 0), default_value.get('y', 0))
                elif hasattr(default_value, "__iter__") and len(default_value) >= 2:
                    default_val = default_value
            else:
                default_val = (0, 0)
            widget = Vec2PropertyWidget(display_name, default_val)
            if isinstance(prop_value, dict):
                widget.set_value((prop_value.get('x', 0), prop_value.get('y', 0)))
            else:
                widget.set_value(prop_value)
            return widget
        elif prop_type == 5 or (prop_type is None and isinstance(prop_value, dict) and 'x' in prop_value and 'y' in prop_value and 'z' in prop_value and 'w' not in prop_value):
            # Vec3
            # Convert default value if it's a dict
            default_val = None
            if default_value is not None:
                if isinstance(default_value, dict):
                    default_val = (default_value.get('x', 0), default_value.get('y', 0), default_value.get('z', 0))
                elif hasattr(default_value, "__iter__") and len(default_value) >= 3:
                    default_val = default_value
            else:
                default_val = (0, 0, 0)
            widget = Vec3PropertyWidget(display_name, default_val)
            if isinstance(prop_value, dict):
                widget.set_value((prop_value.get('x', 0), prop_value.get('y', 0), prop_value.get('z', 0)))
            else:
                widget.set_value(prop_value)
            return widget
        elif prop_type == 6 or (
            prop_type is None
            and isinstance(prop_value, dict)
            and 'x' in prop_value
            and 'y' in prop_value
            and 'z' in prop_value
            and 'w' in prop_value
        ):
            # Vec4 (e.g., quaternion) - but check if it's a linked property first
            if not prop_name.endswith('Linked') and prop_name in ['cornerRadius', 'borderWidth']:
                # This is a linked property - use special widget
                # Convert default value if it's a dict
                default_val = (0, 0, 0, 0)  # Default fallback
                if default_value is not None:
                    if isinstance(default_value, dict):
                        default_val = (default_value.get('x', 0), default_value.get('y', 0),
                                     default_value.get('z', 0), default_value.get('w', 0))
                    elif hasattr(default_value, "__iter__") and len(default_value) >= 4:
                        default_val = default_value
                widget = LinkedVec4PropertyWidget(display_name, f"{prop_name}Linked", default_val)
                if isinstance(prop_value, dict):
                    widget.set_value((
                        prop_value.get('x', 0),
                        prop_value.get('y', 0),
                        prop_value.get('z', 0),
                        prop_value.get('w', 0),
                    ))
                elif hasattr(prop_value, "__iter__") and len(prop_value) >= 4:
                    widget.set_value(prop_value)
                return widget

            # Regular Vec4
            # Convert default value if it's a dict
            default_val = None
            if default_value is not None:
                if isinstance(default_value, dict):
                    default_val = (default_value.get('x', 0), default_value.get('y', 0),
                                 default_value.get('z', 0), default_value.get('w', 0))
                elif hasattr(default_value, "__iter__") and len(default_value) >= 4:
                    default_val = default_value
            else:
                default_val = (0, 0, 0, 0)
            widget = Vec4PropertyWidget(display_name, default_val)
            if isinstance(prop_value, dict):
                widget.set_value(
                    (
                        prop_value.get('x', 0),
                        prop_value.get('y', 0),
                        prop_value.get('z', 0),
                        prop_value.get('w', 0),
                    )
                )
            else:
                widget.set_value(prop_value)
            return widget
        elif isinstance(prop_value, (tuple, list)):
            if len(prop_value) == 2:
                widget = Vec2PropertyWidget(display_name, default_value if default_value is not None else (0, 0))
                widget.set_value(prop_value)
                return widget
            elif len(prop_value) == 3:
                widget = Vec3PropertyWidget(display_name, default_value if default_value is not None else (0, 0, 0))
                widget.set_value(prop_value)
                return widget
            elif len(prop_value) == 4:
                # Could be Vec4 or Color - check prop name
                if "color" in prop_name.lower():
                    widget = ColorPropertyWidget(display_name, default_value if default_value is not None else (1, 1, 1, 1))
                    widget.set_value(prop_value)
                    return widget
                else:
                    widget = Vec4PropertyWidget(display_name, default_value if default_value is not None else (0, 0, 0, 0))
                    widget.set_value(prop_value)
                    return widget

        # Default: string widget
        widget = StringPropertyWidget(display_name, default_value if default_value is not None else "")
        widget.set_value(str(prop_value))
        return widget

    def _on_name_changed(self, new_name):
        """Handle node name change with debouncing"""
        if self.current_node and new_name:
            # Restart timer for debouncing
            self.update_timer.stop()
            self.update_timer.start(500)  # 500ms delay

    def _apply_name_change(self):
        """Apply the name change to the node"""
        if self.current_node:
            new_name = self.node_name_edit.text()
            old_name = self.current_node.get_name()
            if new_name != old_name:
                self.current_node.set_name(new_name)

                # Mark scene as dirty
                if self.editor_bridge:
                    self.editor_bridge.mark_scene_dirty()

                # Auto hot-reload: push the rename into any running play instances.
                runtime = self._live_runtime()
                if runtime is not None:
                    uuid_str = self._node_uuid_str(self.current_node)
                    if uuid_str:
                        runtime.push_node_rename(uuid_str, new_name)

                # Update scene tree
                if self.main_editor and 'scene_tree' in self.main_editor.panels:
                    self.main_editor.panels['scene_tree'].refresh_tree()

    def _live_runtime(self):
        """Return the RuntimeController if any play instances are running, so
        property edits can be pushed to them for automatic hot-reload."""
        main_editor = getattr(self, 'main_editor', None)
        runtime = getattr(main_editor, 'runtime', None) if main_editor else None
        if runtime is None:
            return None
        try:
            if not runtime.is_running():
                return None
        except Exception:
            return None
        return runtime

    @staticmethod
    def _node_uuid_str(node) -> str:
        try:
            return node.get_uuid().to_string()
        except Exception:
            return ""

    @staticmethod
    def _component_index_in(node, component) -> int:
        """Index of `component` within `node`'s component list, or -1. The runtime
        resolves components by this index, which matches because both processes
        deserialize a node's components in array order."""
        try:
            components = node.get_components()
        except Exception:
            return -1

        # Primary: object identity. pybind returns the same Python wrapper for a
        # given C++ shared_ptr, so the component handed to the value-changed
        # signal compares equal to the one re-fetched here - no new binding
        # required.
        for i, comp in enumerate(components):
            if comp is component or comp == component:
                return i

        # Fallback: match by UUID (requires the engine Component.get_uuid binding).
        try:
            target_uuid = component.get_uuid().to_string()
        except Exception:
            return -1
        for i, comp in enumerate(components):
            try:
                if comp.get_uuid().to_string() == target_uuid:
                    return i
            except Exception:
                continue
        return -1

    def push_transform_live(self) -> None:
        """Push the current node's transform (position/rotation/scale/z_index) to
        running instances. Called after a viewport gizmo drag, which edits the
        node directly in the C++ scene without routing through the inspector's
        per-widget value_changed signals."""
        runtime = self._live_runtime()
        if runtime is None or not self.current_node:
            return
        uuid_str = self._node_uuid_str(self.current_node)
        if not uuid_str:
            return
        try:
            props_json = self.editor_bridge.get_node_properties(self.current_node)
            data = json.loads(props_json) if isinstance(props_json, str) else (props_json or {})
            properties = data.get("properties", {}) if isinstance(data, dict) else {}
        except Exception:
            return
        if not isinstance(properties, dict):
            return
        for name in ("position", "rotation", "scale", "z_index"):
            value = properties.get(name)
            if value is not None:
                runtime.push_node_property(uuid_str, name, value)

    def _push_node_live(self, prop_name, value, node=None) -> None:
        """Push a node property change to running instances (no-op if none)."""
        runtime = self._live_runtime()
        node = node or self.current_node
        if runtime is None or not node:
            return
        uuid_str = self._node_uuid_str(node)
        if uuid_str:
            runtime.push_node_property(uuid_str, prop_name, value)

    def _push_component_live(self, component, prop_name, value, owner_node=None) -> None:
        """Push a component property change to running instances (no-op if none)."""
        runtime = self._live_runtime()
        node = owner_node or self.current_node
        if runtime is None or component is None or not node:
            return
        uuid_str = self._node_uuid_str(node)
        index = self._component_index_in(node, component)
        if uuid_str and index >= 0:
            runtime.push_component_property(uuid_str, index, prop_name, value)

    def _on_node_property_changed(self, prop_name, value):
        """Handle a node property change (transform + node-type properties).

        In multi-selection mode the same value is written to every selected node,
        so editing one field updates the whole selection at once."""
        if not self.editor_bridge:
            return
        targets = self._node_targets()
        if not targets:
            return

        try:
            payload = value
            # Convert tuples/lists from vector widgets into dicts expected by C++ side
            if isinstance(value, (tuple, list)):
                if len(value) == 2:
                    payload = {"x": float(value[0]), "y": float(value[1])}
                elif len(value) == 3:
                    payload = {"x": float(value[0]), "y": float(value[1]), "z": float(value[2])}
                elif len(value) == 4:
                    payload = {
                        "x": float(value[0]),
                        "y": float(value[1]),
                        "z": float(value[2]),
                        "w": float(value[3]),
                    }
            elif isinstance(value, dict):
                # Already a dict (from EulerRotationWidget quaternion output)
                payload = value

            value_json = json.dumps(payload)
            for node in targets:
                self.editor_bridge.set_node_property(node, prop_name, value_json)
                # Auto hot-reload: push the change into any running play instances.
                self._push_node_live(prop_name, payload, node)
            # Mark scene as dirty so changes are saved and rendered
            self.editor_bridge.mark_scene_dirty()
        except Exception as e:
            QMessageBox.warning(self, "Error", f"Failed to set node property: {str(e)}")

    def _on_component_property_changed(self, component, prop_name, value):
        """Handle a component property change.

        In multi-selection mode this broadcasts the same edit to the matched peer
        component on every selected node; otherwise it targets the one component."""
        for comp, owner in self._component_targets(component):
            self._apply_component_property(comp, prop_name, value, owner)

    def _apply_component_property(self, component, prop_name, value, owner_node=None):
        """Persist a single component's property change - the unit of work that
        _on_component_property_changed broadcasts across the selection."""
        if not self.editor_bridge or not component:
            return

        try:
            # Handle special linked Vec4 property format
            if isinstance(value, dict) and 'vec4' in value and 'linked' in value:
                # This is from a LinkedVec4PropertyWidget
                vec4_value = value['vec4']
                is_linked = value['linked']
                
                # Set the Vec4 property
                value_json = json.dumps({
                    "x": float(vec4_value[0]),
                    "y": float(vec4_value[1]),
                    "z": float(vec4_value[2]),
                    "w": float(vec4_value[3]),
                })
                self.editor_bridge.set_component_property(component, prop_name, value_json)

                # Set the linked property
                linked_prop_name = f"{prop_name}Linked"
                linked_json = json.dumps(is_linked)
                self.editor_bridge.set_component_property(component, linked_prop_name, linked_json)

                # Mark scene as dirty
                self.editor_bridge.mark_scene_dirty()
                self._push_component_live(component, prop_name, json.loads(value_json), owner_node)
                self._push_component_live(component, linked_prop_name, is_linked, owner_node)
                return
            
            # Handle special linked color property format (4 colors with linked state)
            if isinstance(value, dict) and 'colors' in value and 'linked' in value and 'individual_names' in value:
                # This is from a LinkedColorPropertyWidget
                colors = value['colors']
                is_linked = value['linked']
                individual_names = value['individual_names']
                
                # Set each individual color property
                for i, prop_name_individual in enumerate(individual_names):
                    if i < len(colors):
                        color = colors[i]
                        color_dict = {
                            "r": float(color[0]),
                            "g": float(color[1]),
                            "b": float(color[2]),
                            "a": float(color[3]),
                        }
                        self.editor_bridge.set_component_property(
                            component, prop_name_individual, json.dumps(color_dict))
                        self._push_component_live(component, prop_name_individual, color_dict, owner_node)

                # Set the linked property
                linked_prop_name = f"{prop_name.replace(' ', '')}Linked"  # Remove spaces from display name
                linked_json = json.dumps(is_linked)
                self.editor_bridge.set_component_property(component, linked_prop_name, linked_json)

                # Mark scene as dirty
                self.editor_bridge.mark_scene_dirty()
                self._push_component_live(component, linked_prop_name, is_linked, owner_node)
                return
            
            # Array-shaped collection values arrive wrapped in a sentinel so this
            # path does not coerce 2/3/4-length lists into vectors. Store the raw list.
            if isinstance(value, dict) and COLLECTION_SENTINEL_KEY in value:
                collection_value = value[COLLECTION_SENTINEL_KEY]
                self.editor_bridge.set_component_property(
                    component, prop_name, json.dumps(collection_value))
                self.editor_bridge.mark_scene_dirty()
                self._push_component_live(component, prop_name, collection_value, owner_node)
                return

            # Convert value to JSON - handle different types
            if isinstance(value, bool):
                # Ensure booleans are properly serialized
                value_json = json.dumps(value)
            elif isinstance(value, (tuple, list)):
                # Convert tuples/lists to dicts for vector types
                if len(value) == 2:
                    value_json = json.dumps({"x": float(value[0]), "y": float(value[1])})
                elif len(value) == 3:
                    value_json = json.dumps({"x": float(value[0]), "y": float(value[1]), "z": float(value[2])})
                elif len(value) == 4:
                    # Check if this is a color property (colors use r,g,b,a instead of x,y,z,w)
                    if "color" in prop_name.lower() or "modula" in prop_name.lower() or "tint" in prop_name.lower():
                        value_json = json.dumps({
                            "r": float(value[0]),
                            "g": float(value[1]),
                            "b": float(value[2]),
                            "a": float(value[3]),
                        })
                    else:
                        # Treat as Vec4
                        value_json = json.dumps({
                            "x": float(value[0]),
                            "y": float(value[1]),
                            "z": float(value[2]),
                            "w": float(value[3]),
                        })
                else:
                    value_json = json.dumps(value)
            else:
                value_json = json.dumps(value)

            self.editor_bridge.set_component_property(component, prop_name, value_json)
            # Mark scene as dirty
            self.editor_bridge.mark_scene_dirty()
            # Auto hot-reload: push the change into any running play instances.
            self._push_component_live(component, prop_name, json.loads(value_json))

            # If this is an Image2D and texturePath changed, refresh inspector to pick up width/height
            if prop_name == "texturePath":
                component_type = self.editor_bridge.get_component_type(component) if hasattr(self.editor_bridge, 'get_component_type') else None
                if component_type == "Image2D" or component_type is None:
                    # Deferred refresh to pick up auto-set width/height from texture
                    from PyQt6.QtCore import QTimer
                    QTimer.singleShot(100, self.refresh_properties)

            # If this is a script component and script_path changed, reload the script and refresh inspector
            if prop_name == "script_path":
                component_type = component.get_type_name()
                if component_type in ('LuaScriptComponent', 'PythonScriptComponent', 'MRubyScriptComponent'):
                    # Reload the script component to parse new export properties
                    from PyQt6.QtCore import QTimer
                    # Capture component in closure explicitly
                    comp = component
                    def reload_and_refresh(c=comp):
                        if self.main_editor and 'console' in self.main_editor.panels:
                            self.main_editor.panels['console'].log_message(
                                f"Reloading script component after script_path change: {c.get_name()}", "Debug")
                        if hasattr(self.editor_bridge, 'reload_script_component'):
                            result = self.editor_bridge.reload_script_component(c)
                            if self.main_editor and 'console' in self.main_editor.panels:
                                self.main_editor.panels['console'].log_message(
                                    f"reload_script_component result: {result}", "Debug")
                        self.refresh_properties()
                    QTimer.singleShot(100, reload_and_refresh)

            # If this is a StaticMesh3D and modelPath changed, refresh material slots
            if prop_name == "modelPath" and component in self._material_widgets:
                if self.main_editor and 'console' in self.main_editor.panels:
                    self.main_editor.panels['console'].log_message(
                        "[Inspector] modelPath changed, scheduling deferred refresh for material slots widget", "Debug")
                material_widget = self._material_widgets[component]
                # Reset retry count and trigger deferred refresh with retry logic
                if hasattr(material_widget, '_deferred_refresh'):
                    material_widget._retry_count = 0
                    from PyQt6.QtCore import QTimer
                    QTimer.singleShot(200, material_widget._deferred_refresh)
        except Exception as e:
            QMessageBox.warning(self, "Error", f"Failed to set property: {str(e)}")


    def _on_duplicate_component_clicked(self, component):
        """Add a second component of the same type with identical properties to the node."""
        if not self.current_node or not self.editor_bridge or not component:
            return

        try:
            component_type = component.get_type_name()

            new_component = self.editor_bridge.create_component(component_type)
            if not new_component:
                QMessageBox.critical(
                    self, "Error", f"Failed to create component of type: {component_type}")
                return

            properties_json_str = self.editor_bridge.get_component_properties(component)
            component_data = json.loads(properties_json_str)

            try:
                self.editor_bridge.deserialize_component_direct(
                    new_component, json.dumps(component_data))
            except Exception:
                if "properties" in component_data and isinstance(component_data["properties"], dict):
                    properties = component_data["properties"]
                else:
                    properties = component_data

                for prop_name, prop_value in properties.items():
                    if prop_name in ['uuid', 'enabled']:
                        continue
                    try:
                        self.editor_bridge.set_component_property_direct(
                            new_component, prop_name, json.dumps(prop_value))
                    except Exception:
                        pass

            try:
                new_component.set_enabled(component.is_enabled())
            except Exception:
                pass

            # Add through the command system so the whole duplicate is one undoable step.
            # Properties were configured above on the detached component, so the
            # AddComponentCommand captures the fully-populated instance.
            self.editor_bridge.add_component(self.current_node, new_component)

            self.editor_bridge.mark_scene_dirty()
            self.refresh_properties()

            if self.main_editor and 'console' in self.main_editor.panels:
                self.main_editor.panels['console'].log_message(
                    f"Duplicated component '{component_type}' on '{self.current_node.get_name()}'",
                    "Info",
                )
        except Exception as e:
            QMessageBox.critical(self, "Error", f"Error duplicating component: {str(e)}")

    def _on_remove_component_clicked(self, component):
        """Handle remove component button click."""
        if not self.current_node or not self.editor_bridge or not component:
            return

        try:
            if self.editor_bridge.remove_component(self.current_node, component):
                # Mark scene as dirty and refresh inspector
                self.editor_bridge.mark_scene_dirty()
                self.refresh_properties()

                # Emit signal for anyone interested
                self.component_removed.emit(component)

                # Log to console if available
                if self.main_editor and 'console' in self.main_editor.panels:
                    self.main_editor.panels['console'].log_message(
                        f"Removed component '{component.get_type_name()}' from '{self.current_node.get_name()}'",
                        "Info",
                    )
            else:
                QMessageBox.critical(self, "Error", "Failed to remove component from node")
        except Exception as e:
            QMessageBox.critical(self, "Error", f"Error removing component: {str(e)}")

    def _on_add_component_clicked(self):
        """Handle add component button click"""
        if not self.current_node or not self.editor_bridge:
            return

        dialog = AddComponentDialog(self.editor_bridge, self)
        if dialog.exec():
            component_type = dialog.get_selected_type()
            if component_type:
                try:
                    # Create and add component
                    component = self.editor_bridge.create_component(component_type)
                    if component:
                        if self.editor_bridge.add_component(self.current_node, component):
                            # Mark scene as dirty
                            self.editor_bridge.mark_scene_dirty()

                            # Refresh inspector
                            self.refresh_properties()

                            if self.main_editor and 'console' in self.main_editor.panels:
                                self.main_editor.panels['console'].log_message(
                                    f"Added component '{component_type}' to '{self.current_node.get_name()}'", "Info")
                        else:
                            QMessageBox.critical(self, "Error", "Failed to add component to node")
                    else:
                        QMessageBox.critical(self, "Error", f"Failed to create component of type: {component_type}")
                except Exception as e:
                    QMessageBox.critical(self, "Error", f"Error adding component: {str(e)}")
