"""
Plugin manifest parsing and validation.

A Lupine editor plugin is a folder placed inside a project's ``plugins/``
directory. The folder is recognized as a plugin when it contains a
``plugin.json`` manifest. The manifest declares metadata, the editor entry
script, and (optionally) game-side autoload singletons that should be
registered while the plugin is enabled.
"""

from __future__ import annotations

import json
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Dict, List, Optional

MANIFEST_FILENAME: str = "plugin.json"
MANIFEST_FORMAT_VERSION: str = "1.0"


class PluginManifestError(Exception):
    """Raised when a plugin manifest is missing required data or malformed."""


@dataclass
class PluginAutoload:
    """A game-side singleton declared by a plugin.

    ``script_path`` is interpreted relative to the plugin folder. When the
    plugin is enabled the manager resolves it to a project-relative path and
    registers it in the project's ``globals.json`` so the runtime loads it.
    """

    name: str
    script_path: str

    @staticmethod
    def from_dict(data: Dict[str, Any]) -> "PluginAutoload":
        name: str = str(data.get("name", "")).strip()
        script_path: str = str(data.get("script_path", data.get("path", ""))).strip()
        if not name:
            raise PluginManifestError("Autoload entry is missing a 'name'.")
        if not name.isidentifier():
            raise PluginManifestError(
                f"Autoload name '{name}' is not a valid identifier."
            )
        if not script_path:
            raise PluginManifestError(
                f"Autoload '{name}' is missing a 'script_path'."
            )
        return PluginAutoload(name=name, script_path=script_path.replace("\\", "/"))


@dataclass
class PluginManifest:
    """Parsed representation of a ``plugin.json`` file."""

    plugin_id: str
    name: str
    directory: Path
    description: str = ""
    author: str = ""
    version: str = "1.0.0"
    editor_script: Optional[str] = None
    editor_class: Optional[str] = None
    autoloads: List[PluginAutoload] = field(default_factory=list)
    raw: Dict[str, Any] = field(default_factory=dict)

    @property
    def manifest_path(self) -> Path:
        return self.directory / MANIFEST_FILENAME

    @property
    def has_editor_extension(self) -> bool:
        return bool(self.editor_script)

    def editor_script_path(self) -> Optional[Path]:
        if not self.editor_script:
            return None
        return self.directory / self.editor_script

    @staticmethod
    def load(directory: Path) -> "PluginManifest":
        """Load and validate the manifest from a plugin directory.

        Raises ``PluginManifestError`` if the manifest is missing, unreadable,
        or fails validation.
        """
        directory = Path(directory)
        manifest_path: Path = directory / MANIFEST_FILENAME
        if not manifest_path.is_file():
            raise PluginManifestError(
                f"No {MANIFEST_FILENAME} found in '{directory}'."
            )

        try:
            with open(manifest_path, "r", encoding="utf-8") as handle:
                data: Dict[str, Any] = json.load(handle)
        except (OSError, json.JSONDecodeError) as exc:
            raise PluginManifestError(
                f"Failed to read '{manifest_path}': {exc}"
            ) from exc

        if not isinstance(data, dict):
            raise PluginManifestError(
                f"Manifest '{manifest_path}' must contain a JSON object."
            )

        plugin_id: str = str(data.get("id", "") or directory.name).strip()
        if not plugin_id:
            raise PluginManifestError("Plugin 'id' could not be determined.")

        name: str = str(data.get("name", plugin_id)).strip() or plugin_id

        editor_script: Optional[str] = data.get("editor_script")
        if editor_script is not None:
            editor_script = str(editor_script).strip() or None

        editor_class: Optional[str] = data.get("editor_class")
        if editor_class is not None:
            editor_class = str(editor_class).strip() or None

        autoloads: List[PluginAutoload] = []
        raw_autoloads: Any = data.get("autoloads", [])
        if isinstance(raw_autoloads, list):
            for entry in raw_autoloads:
                if isinstance(entry, dict):
                    autoloads.append(PluginAutoload.from_dict(entry))

        return PluginManifest(
            plugin_id=plugin_id,
            name=name,
            directory=directory,
            description=str(data.get("description", "")).strip(),
            author=str(data.get("author", "")).strip(),
            version=str(data.get("version", "1.0.0")).strip() or "1.0.0",
            editor_script=editor_script,
            editor_class=editor_class,
            autoloads=autoloads,
            raw=data,
        )
