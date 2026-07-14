"""
Built-in custom inspector widgets.

These demonstrate the registry contract and cover common authoring needs that the
type-based defaults do not. A property opts in by declaring a custom widget id, e.g.
in a script export:

    --[Custom("slider", "{\"min\": 0, \"max\": 100, \"step\": 1}")]
    --@export volume float 50.0

    --[Custom("password")]
    --@export api_key string ""

Every widget here is PropertyWidget-compatible: it exposes ``value_changed`` and
``set_value`` / ``get_value`` so the inspector wires it up like any other row.
"""

from PyQt6.QtWidgets import QHBoxLayout, QSlider, QDoubleSpinBox, QLineEdit
from PyQt6.QtCore import Qt

try:
    from editor.panels.inspector_panel import PropertyWidget
except ImportError:
    from panels.inspector_panel import PropertyWidget

try:
    from editor.widgets.custom_widget_registry import register_inspector_widget_factory
except ImportError:
    from widgets.custom_widget_registry import register_inspector_widget_factory


class SliderPropertyWidget(PropertyWidget):
    """A numeric value edited with a horizontal slider plus a spin box.

    Config keys: ``min`` (default 0), ``max`` (default 100), ``step`` (default 1),
    ``integer`` (bool, default False).
    """

    def __init__(self, property_name, default_value, config, parent=None):
        self._min = float(config.get("min", 0.0))
        self._max = float(config.get("max", 100.0))
        self._step = float(config.get("step", 1.0)) or 1.0
        self._integer = bool(config.get("integer", False))
        super().__init__(property_name, default_value, parent)

    def setup_ui(self):
        layout = QHBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(6)
        layout.addWidget(self._make_label())

        self._slider = QSlider(Qt.Orientation.Horizontal)
        self._ticks = max(1, int(round((self._max - self._min) / self._step)))
        self._slider.setRange(0, self._ticks)
        self._slider.valueChanged.connect(self._on_slider)

        self._spin = QDoubleSpinBox()
        self._spin.setDecimals(0 if self._integer else 3)
        self._spin.setRange(self._min, self._max)
        self._spin.setSingleStep(self._step)
        self._spin.valueChanged.connect(self._on_spin)
        self._spin.setMaximumWidth(90)

        layout.addWidget(self._slider, 1)
        layout.addWidget(self._spin)
        layout.addWidget(self._create_reset_button())

    def _value_to_tick(self, value):
        if self._max == self._min:
            return 0
        ratio = (float(value) - self._min) / (self._max - self._min)
        return int(round(ratio * self._ticks))

    def _tick_to_value(self, tick):
        value = self._min + (tick / self._ticks) * (self._max - self._min)
        return int(round(value)) if self._integer else value

    def _on_slider(self, tick):
        value = self._tick_to_value(tick)
        self._spin.blockSignals(True)
        self._spin.setValue(value)
        self._spin.blockSignals(False)
        self.value_changed.emit(value)

    def _on_spin(self, value):
        result = int(round(value)) if self._integer else value
        self._slider.blockSignals(True)
        self._slider.setValue(self._value_to_tick(result))
        self._slider.blockSignals(False)
        self.value_changed.emit(result)

    def set_value(self, value):
        if value is None:
            return
        try:
            numeric = float(value)
        except (ValueError, TypeError):
            return
        self._spin.blockSignals(True)
        self._slider.blockSignals(True)
        self._spin.setValue(numeric)
        self._slider.setValue(self._value_to_tick(numeric))
        self._spin.blockSignals(False)
        self._slider.blockSignals(False)

    def get_value(self):
        value = self._spin.value()
        return int(round(value)) if self._integer else value


class PasswordPropertyWidget(PropertyWidget):
    """A string edited through a masked line edit (for secrets/keys/tokens)."""

    def setup_ui(self):
        layout = QHBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(6)
        layout.addWidget(self._make_label())

        self._edit = QLineEdit()
        self._edit.setEchoMode(QLineEdit.EchoMode.Password)
        self._edit.textChanged.connect(lambda text: self.value_changed.emit(text))
        layout.addWidget(self._edit, 1)
        layout.addWidget(self._create_reset_button())

    def set_value(self, value):
        self._edit.blockSignals(True)
        self._edit.setText(str(value) if value is not None else "")
        self._edit.blockSignals(False)

    def get_value(self):
        return self._edit.text()


def _slider_factory(property_name, metadata, value, config):
    default = metadata.get("defaultValue", metadata.get("default"))
    widget = SliderPropertyWidget(property_name, default, config)
    widget.set_value(value if value is not None else default)
    return widget


def _password_factory(property_name, metadata, value, config):
    default = metadata.get("defaultValue", metadata.get("default", ""))
    widget = PasswordPropertyWidget(property_name, default)
    widget.set_value(value if value is not None else default)
    return widget


register_inspector_widget_factory("slider", _slider_factory)
register_inspector_widget_factory("password", _password_factory)
