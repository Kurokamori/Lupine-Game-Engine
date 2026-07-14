"""
Themed vector icons for the inspector.

Qt's QSS triangle-via-border trick renders unreliably for spin-box / combo sub-controls
(it can show up as a blank white square). Instead we generate small SVG glyphs, tinted to
the active theme color, cache them on disk, and reference them by path - both from QSS
(``image: url(...)``) and as ``QIcon`` for buttons. Regenerating with a different color
just writes a new cache file, so icons follow theme changes automatically.
"""

from __future__ import annotations

import hashlib
import tempfile
from pathlib import Path
from typing import Dict

from PyQt6.QtGui import QIcon


_CACHE_DIR = Path(tempfile.gettempdir()) / "lupine_engine_inspector_icons"


# Minimal single-stroke glyphs. `{color}` is the only template field.
_TEMPLATES: Dict[str, str] = {
    "chevron-up": (
        '<svg xmlns="http://www.w3.org/2000/svg" width="12" height="12" viewBox="0 0 12 12">'
        '<polyline points="3,7.25 6,4.25 9,7.25" fill="none" stroke="{color}" '
        'stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/></svg>'
    ),
    "chevron-down": (
        '<svg xmlns="http://www.w3.org/2000/svg" width="12" height="12" viewBox="0 0 12 12">'
        '<polyline points="3,4.75 6,7.75 9,4.75" fill="none" stroke="{color}" '
        'stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/></svg>'
    ),
    "revert": (
        '<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 16 16">'
        '<path d="M4.5 6.5 H9.5 a3.5 3.5 0 0 1 0 7 H7" fill="none" stroke="{color}" '
        'stroke-width="1.4" stroke-linecap="round" stroke-linejoin="round"/>'
        '<polyline points="6.6,4 4,6.5 6.6,9" fill="none" stroke="{color}" '
        'stroke-width="1.4" stroke-linecap="round" stroke-linejoin="round"/></svg>'
    ),
    "folder": (
        '<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 16 16">'
        '<path d="M2 4.6 h3.8 l1.3 1.6 H14 v7 H2 Z" fill="none" stroke="{color}" '
        'stroke-width="1.3" stroke-linejoin="round"/></svg>'
    ),
    "edit": (
        '<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 16 16">'
        '<path d="M10.5 2.8 L13.2 5.5 L6 12.7 L3 13.5 L3.8 10.5 Z" fill="none" stroke="{color}" '
        'stroke-width="1.3" stroke-linecap="round" stroke-linejoin="round"/>'
        '<line x1="9.3" y1="4" x2="12" y2="6.7" stroke="{color}" '
        'stroke-width="1.3" stroke-linecap="round"/></svg>'
    ),
    "duplicate": (
        '<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 16 16">'
        '<rect x="5.5" y="5.5" width="8" height="8" rx="1.4" fill="none" stroke="{color}" '
        'stroke-width="1.3" stroke-linejoin="round"/>'
        '<path d="M3.5 10.5 H3 a0.5 0.5 0 0 1 -0.5 -0.5 V3 a0.5 0.5 0 0 1 0.5 -0.5 H10 '
        'a0.5 0.5 0 0 1 0.5 0.5 V3.5" fill="none" stroke="{color}" '
        'stroke-width="1.3" stroke-linecap="round" stroke-linejoin="round"/></svg>'
    ),
    "close": (
        '<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 16 16">'
        '<line x1="4.5" y1="4.5" x2="11.5" y2="11.5" stroke="{color}" '
        'stroke-width="1.4" stroke-linecap="round"/>'
        '<line x1="11.5" y1="4.5" x2="4.5" y2="11.5" stroke="{color}" '
        'stroke-width="1.4" stroke-linecap="round"/></svg>'
    ),
    "plus": (
        '<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 16 16">'
        '<line x1="8" y1="3.5" x2="8" y2="12.5" stroke="{color}" '
        'stroke-width="1.5" stroke-linecap="round"/>'
        '<line x1="3.5" y1="8" x2="12.5" y2="8" stroke="{color}" '
        'stroke-width="1.5" stroke-linecap="round"/></svg>'
    ),
    "image": (
        '<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 16 16">'
        '<rect x="2.5" y="3.5" width="11" height="9" rx="1.2" fill="none" stroke="{color}" '
        'stroke-width="1.3" stroke-linejoin="round"/>'
        '<circle cx="6" cy="6.5" r="1.1" fill="{color}"/>'
        '<path d="M3 11.5 L6.5 8 L9 10.5 L11 8.5 L13 11" fill="none" stroke="{color}" '
        'stroke-width="1.3" stroke-linecap="round" stroke-linejoin="round"/></svg>'
    ),
}


def _write(name: str, color: str) -> Path:
    template = _TEMPLATES[name]
    _CACHE_DIR.mkdir(parents=True, exist_ok=True)
    key = hashlib.md5(f"{name}:{color}".encode("utf-8")).hexdigest()[:10]
    path = _CACHE_DIR / f"{name}_{key}.svg"
    if not path.exists():
        path.write_text(template.format(color=color), encoding="utf-8")
    return path


def svg_path(name: str, color: str) -> str:
    """Absolute filesystem path to the tinted glyph (created on first use)."""
    return str(_write(name, color))


def qss_url(name: str, color: str) -> str:
    """Forward-slashed path suitable for a QSS ``image: url("...")`` rule."""
    return _write(name, color).as_posix()


_ICON_CACHE: Dict[str, QIcon] = {}


def icon(name: str, color: str) -> QIcon:
    """A ``QIcon`` of the tinted glyph for use on buttons.

    Cached per (name, color): building the icon otherwise re-hashes the key, hits the
    filesystem (``exists``) and re-decodes the SVG on every call. The inspector asks for
    the same few glyphs once per component on each selection, so the cache turns those
    repeated disk reads into a dict lookup.
    """
    cache_key = f"{name}:{color}"
    cached = _ICON_CACHE.get(cache_key)
    if cached is None:
        cached = QIcon(svg_path(name, color))
        _ICON_CACHE[cache_key] = cached
    return cached
