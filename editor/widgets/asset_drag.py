"""
Shared asset drag-and-drop plumbing.

Provides a single canonical MIME type for dragging project assets (images, audio,
.ares archetype instances, scenes, scripts, ...) out of the file browser and onto
inspector reference fields, the way Unity drags assets onto an ObjectField.

The payload is one or more ``res://`` paths. External OS file drops (``text/uri-list``)
are accepted too and converted to ``res://`` when they fall inside the project.

Path <-> res:// conversion reuses the canonical converters in the inspector panel
(lazily imported to avoid an import cycle: the inspector imports this module).
"""

from PyQt6.QtCore import QMimeData, QUrl

# Canonical internal MIME for a list of project asset paths. The data is the
# newline-joined res:// paths, UTF-8 encoded.
LUPINE_ASSET_MIME = "application/x-lupine-asset"

# Recognised asset categories (lowercase extensions, with leading dot).
IMAGE_EXTENSIONS = (
    ".png", ".jpg", ".jpeg", ".bmp", ".gif", ".webp", ".tga",
    ".dds", ".ktx", ".hdr", ".exr", ".psd", ".tiff", ".tif",
)
AUDIO_EXTENSIONS = (
    ".wav", ".mp3", ".ogg", ".flac", ".opus", ".aiff", ".aif", ".mod", ".xm",
)
ARCHETYPE_EXTENSIONS = (".ares",)
SCENE_EXTENSIONS = (".scene", ".lupscene")
PREFAB_EXTENSIONS = (".prefab",)
SCRIPT_EXTENSIONS = (".lua", ".py", ".rb")
MODEL_EXTENSIONS = (".gltf", ".glb", ".obj", ".fbx", ".dae")
# Extensions QPixmap can decode for an inspector thumbnail.
PREVIEWABLE_EXTENSIONS = (
    ".png", ".jpg", ".jpeg", ".bmp", ".gif", ".webp", ".tiff", ".tif",
)


def _inspector_module():
    """The inspector panel module, regardless of how the editor package is rooted."""
    try:
        from editor.panels import inspector_panel as ip
    except ImportError:
        from panels import inspector_panel as ip
    return ip


def to_res_path(absolute_or_res_path: str) -> str:
    """Convert an absolute path to ``res://`` (pass-through if already res:// or external)."""
    if not absolute_or_res_path:
        return ""
    if absolute_or_res_path.startswith("res://"):
        return absolute_or_res_path
    try:
        return _inspector_module().convert_to_res_path(absolute_or_res_path)
    except Exception:
        return absolute_or_res_path


def from_res_path(res_path: str) -> str:
    """Resolve a ``res://`` path to an absolute filesystem path."""
    if not res_path:
        return ""
    try:
        return _inspector_module().convert_from_res_path(res_path)
    except Exception:
        return res_path


def _has_extension(path: str, extensions) -> bool:
    if not path:
        return False
    lowered = path.lower()
    return any(lowered.endswith(ext) for ext in extensions)


def is_image_path(path: str) -> bool:
    return _has_extension(path, IMAGE_EXTENSIONS)


def is_audio_path(path: str) -> bool:
    return _has_extension(path, AUDIO_EXTENSIONS)


def is_archetype_path(path: str) -> bool:
    return _has_extension(path, ARCHETYPE_EXTENSIONS)


def is_previewable_image(path: str) -> bool:
    return _has_extension(path, PREVIEWABLE_EXTENSIONS)


def matches_extensions(path: str, extensions) -> bool:
    """Whether ``path`` matches the given iterable of extensions.

    Extensions may be given with or without a leading dot; an empty/None iterable
    means "accept anything".
    """
    if not extensions:
        return True
    normalised = tuple(
        (ext if ext.startswith(".") else "." + ext).lower()
        for ext in extensions if ext
    )
    if not normalised:
        return True
    return _has_extension(path, normalised)


def build_asset_mimedata(res_paths) -> QMimeData:
    """Build a QMimeData carrying one or more project asset paths.

    Sets the canonical internal MIME, a ``text/uri-list`` of resolved local files
    (so external drop targets work) and plain text (the first path).
    """
    paths = [p for p in (res_paths or []) if p]
    mime = QMimeData()
    if not paths:
        return mime
    mime.setData(LUPINE_ASSET_MIME, "\n".join(paths).encode("utf-8"))
    urls = []
    for res in paths:
        absolute = from_res_path(res)
        if absolute:
            urls.append(QUrl.fromLocalFile(absolute))
    if urls:
        mime.setUrls(urls)
    mime.setText(paths[0])
    return mime


def mime_has_assets(mime) -> bool:
    """Whether a QMimeData carries anything we can interpret as asset path(s)."""
    if mime is None:
        return False
    if mime.hasFormat(LUPINE_ASSET_MIME):
        return True
    if mime.hasUrls():
        return any(url.isLocalFile() for url in mime.urls())
    return False


def asset_paths_from_mime(mime) -> list:
    """Extract res:// asset paths from a drop's QMimeData.

    Order of preference: the internal MIME (already res:// paths), then external
    local-file URLs (converted to res:// where inside the project).
    """
    if mime is None:
        return []
    if mime.hasFormat(LUPINE_ASSET_MIME):
        raw = bytes(mime.data(LUPINE_ASSET_MIME)).decode("utf-8", "replace")
        return [line for line in raw.split("\n") if line]
    if mime.hasUrls():
        result = []
        for url in mime.urls():
            if url.isLocalFile():
                result.append(to_res_path(url.toLocalFile()))
        return [p for p in result if p]
    return []
