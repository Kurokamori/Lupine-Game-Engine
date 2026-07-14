"""Built-in custom inspector widgets.

Modules in this package are auto-imported by
``editor.widgets.custom_widget_registry.discover_builtin_widgets`` so they can
self-register their factories. Each module should call
``register_inspector_widget`` / ``register_inspector_widget_factory`` at import time.
"""
