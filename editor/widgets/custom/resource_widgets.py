"""
Built-in asset reference inspector widgets (Unity-style ObjectField parity).

These register custom inspector widget ids that any C++ component, script export or
archetype field can opt into via its ``custom_widget`` metadata. They provide
drag-and-drop asset slots, multi-value lists and collapsible image previews.

Registered ids:

  - ``resource``       single asset reference (any type), drag-and-drop + preview
  - ``resource_list``  multiple asset references
  - ``image``          single image reference (image-only filter, preview on)
  - ``image_list``     multiple image references
  - ``audio``          single audio reference with a play button
  - ``audio_list``     multiple audio references
  - ``archetype``      single .ares reference (config ``{"class": "EnemyStats"}``)
  - ``archetype_list`` multiple .ares references

Config keys (JSON in ``custom_widget_config``), all optional:

  - ``extensions``  list of accepted extensions, e.g. ``[".png", ".jpg"]``
  - ``class``       archetype class name to filter .ares references
  - ``preview``     bool, show image previews (default True)

An archetype field authored as a ``StringArray`` with widget ``image_list`` stores its
value as a JSON array of ``res://`` paths and round-trips through the normal save path.
"""

try:
    from editor.panels.inspector_panel import ResourceFieldWidget, ResourceArrayFieldWidget
except ImportError:
    from panels.inspector_panel import ResourceFieldWidget, ResourceArrayFieldWidget

try:
    from editor.widgets.custom_widget_registry import register_inspector_widget_factory
except ImportError:
    from widgets.custom_widget_registry import register_inspector_widget_factory

try:
    from editor.widgets import asset_drag
except ImportError:
    from widgets import asset_drag


def _display_name(property_name):
    """Title-case a raw field name the way the inspector does for default rows."""
    try:
        from editor.panels.inspector_panel import format_property_name
    except ImportError:
        from panels.inspector_panel import format_property_name
    return format_property_name(property_name)


def _config_extensions(config, fallback=None):
    extensions = config.get("extensions")
    if isinstance(extensions, (list, tuple)) and extensions:
        return [str(e) for e in extensions]
    return fallback


def _make_single(property_name, metadata, value, config,
                 extensions=None, archetype_class="", audio_controls=False,
                 show_preview=True):
    default = metadata.get("defaultValue", metadata.get("default", ""))
    widget = ResourceFieldWidget(
        _display_name(property_name),
        default if default is not None else "",
        extensions=extensions, archetype_class=archetype_class,
        audio_controls=audio_controls,
        show_preview=bool(config.get("preview", show_preview)))
    widget.set_value(value if value is not None else (default or ""))
    return widget


def _make_list(property_name, metadata, value, config,
               extensions=None, archetype_class="", audio_controls=False,
               show_preview=True):
    default = metadata.get("defaultValue", metadata.get("default"))
    widget = ResourceArrayFieldWidget(
        _display_name(property_name),
        default if default is not None else [],
        extensions=extensions, archetype_class=archetype_class,
        audio_controls=audio_controls,
        show_preview=bool(config.get("preview", show_preview)))
    widget.set_value(value if value is not None else (default or []))
    return widget


def _resource_factory(property_name, metadata, value, config):
    return _make_single(property_name, metadata, value, config,
                        extensions=_config_extensions(config))


def _resource_list_factory(property_name, metadata, value, config):
    return _make_list(property_name, metadata, value, config,
                     extensions=_config_extensions(config))


def _image_factory(property_name, metadata, value, config):
    return _make_single(property_name, metadata, value, config,
                        extensions=_config_extensions(config, list(asset_drag.IMAGE_EXTENSIONS)))


def _image_list_factory(property_name, metadata, value, config):
    return _make_list(property_name, metadata, value, config,
                     extensions=_config_extensions(config, list(asset_drag.IMAGE_EXTENSIONS)))


def _audio_factory(property_name, metadata, value, config):
    return _make_single(property_name, metadata, value, config,
                        extensions=_config_extensions(config, list(asset_drag.AUDIO_EXTENSIONS)),
                        audio_controls=True, show_preview=False)


def _audio_list_factory(property_name, metadata, value, config):
    return _make_list(property_name, metadata, value, config,
                     extensions=_config_extensions(config, list(asset_drag.AUDIO_EXTENSIONS)),
                     audio_controls=True, show_preview=False)


def _archetype_factory(property_name, metadata, value, config):
    return _make_single(property_name, metadata, value, config,
                        archetype_class=str(config.get("class", "")), show_preview=False)


def _archetype_list_factory(property_name, metadata, value, config):
    return _make_list(property_name, metadata, value, config,
                     archetype_class=str(config.get("class", "")), show_preview=False)


register_inspector_widget_factory("resource", _resource_factory)
register_inspector_widget_factory("resource_list", _resource_list_factory)
register_inspector_widget_factory("image", _image_factory)
register_inspector_widget_factory("image_list", _image_list_factory)
register_inspector_widget_factory("audio", _audio_factory)
register_inspector_widget_factory("audio_list", _audio_list_factory)
register_inspector_widget_factory("archetype", _archetype_factory)
register_inspector_widget_factory("archetype_list", _archetype_list_factory)
