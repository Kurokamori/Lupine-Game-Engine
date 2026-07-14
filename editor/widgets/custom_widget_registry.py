"""
Custom inspector widget registry.

A property (on a C++ component, a script export, or an archetype field) can declare
that it wants a bespoke editor by carrying a ``custom_widget`` id (and optional
``custom_widget_config`` JSON string) in its metadata. The metadata contract is a
plain string id + string config so it stays decoupled from any particular frontend
(this PyQt editor today, possibly something else later) -- the engine never depends
on the widget implementation, only on the id.

Widgets register a factory under an id, either with the ``@register_inspector_widget``
decorator or via ``register_inspector_widget_factory``. Built-in widgets live under
``editor/widgets/custom/`` and project-supplied widgets are auto-discovered from
``<project>/addons/inspector_widgets/``. A factory has the signature::

    factory(property_name: str,
            metadata: dict,
            value,
            config: dict) -> QWidget | None

and must return a widget exposing a ``value_changed`` signal and ``set_value`` /
``get_value`` methods (i.e. a PropertyWidget-compatible object), or ``None`` to fall
back to the default type-based widget.
"""

import importlib
import importlib.util
import json
import os
import sys
import traceback


# id (str) -> factory callable
_REGISTRY = {}

# Tracks which builtin/addon modules have been imported so discovery is idempotent.
_DISCOVERED_BUILTINS = False
_DISCOVERED_ADDON_DIRS = set()


def register_inspector_widget_factory(widget_id, factory):
    """Register ``factory`` under ``widget_id`` (last registration wins)."""
    if not widget_id or not callable(factory):
        return
    _REGISTRY[str(widget_id)] = factory


def register_inspector_widget(widget_id):
    """Decorator form of :func:`register_inspector_widget_factory`."""
    def _decorator(factory):
        register_inspector_widget_factory(widget_id, factory)
        return factory
    return _decorator


def has_inspector_widget(widget_id):
    return str(widget_id) in _REGISTRY


def registered_widget_ids():
    return sorted(_REGISTRY.keys())


def _parse_config(config):
    """Normalise the config payload (JSON string or dict) to a dict."""
    if config is None or config == "":
        return {}
    if isinstance(config, dict):
        return config
    if isinstance(config, str):
        try:
            parsed = json.loads(config)
            return parsed if isinstance(parsed, dict) else {"value": parsed}
        except (ValueError, TypeError):
            return {"raw": config}
    return {}


def create_inspector_widget(widget_id, property_name, metadata, value, config=None):
    """Build the custom widget registered under ``widget_id``.

    Returns the constructed widget, or ``None`` if the id is unknown or the factory
    declined / raised (so the caller can fall back to the default widget).
    """
    factory = _REGISTRY.get(str(widget_id))
    if factory is None:
        return None
    try:
        return factory(property_name, metadata or {}, value, _parse_config(config))
    except Exception:
        sys.stderr.write(
            "[custom_widget_registry] factory '%s' failed:\n%s\n"
            % (widget_id, traceback.format_exc()))
        return None


def discover_builtin_widgets():
    """Import every module under ``editor/widgets/custom/`` so they self-register."""
    global _DISCOVERED_BUILTINS
    if _DISCOVERED_BUILTINS:
        return
    _DISCOVERED_BUILTINS = True

    custom_dir = os.path.join(os.path.dirname(__file__), "custom")
    if not os.path.isdir(custom_dir):
        return

    for entry in sorted(os.listdir(custom_dir)):
        if not entry.endswith(".py") or entry.startswith("_"):
            continue
        module_stem = entry[:-3]
        for module_name in ("editor.widgets.custom." + module_stem,
                            "widgets.custom." + module_stem):
            try:
                importlib.import_module(module_name)
                break
            except ImportError:
                continue
            except Exception:
                sys.stderr.write(
                    "[custom_widget_registry] error importing builtin '%s':\n%s\n"
                    % (module_name, traceback.format_exc()))
                break


def discover_project_widgets(project_dir):
    """Import every module under ``<project>/addons/inspector_widgets/``.

    Project widgets self-register exactly like builtins. Discovery for a given
    directory runs once per editor session.
    """
    if not project_dir:
        return
    addon_dir = os.path.join(project_dir, "addons", "inspector_widgets")
    addon_dir = os.path.normpath(addon_dir)
    if addon_dir in _DISCOVERED_ADDON_DIRS or not os.path.isdir(addon_dir):
        return
    _DISCOVERED_ADDON_DIRS.add(addon_dir)

    for entry in sorted(os.listdir(addon_dir)):
        if not entry.endswith(".py") or entry.startswith("_"):
            continue
        module_path = os.path.join(addon_dir, entry)
        module_name = "lupine_addon_widget_" + entry[:-3]
        try:
            spec = importlib.util.spec_from_file_location(module_name, module_path)
            if spec is None or spec.loader is None:
                continue
            module = importlib.util.module_from_spec(spec)
            sys.modules[module_name] = module
            spec.loader.exec_module(module)
        except Exception:
            sys.stderr.write(
                "[custom_widget_registry] error importing addon '%s':\n%s\n"
                % (module_path, traceback.format_exc()))
