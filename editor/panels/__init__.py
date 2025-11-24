"""
Lupine Engine Editor Panels
Modular, dockable panels for the main editor interface
"""

from .base_panel import EditorPanel
from .scene_tree_panel import SceneTreePanel
from .asset_browser_panel import AssetBrowserPanel
from .file_browser_panel import FileBrowserPanel
from .inspector_panel import InspectorPanel
from .script_editor_panel import ScriptEditorPanel
from .console_panel import ConsolePanel

__all__ = [
    'EditorPanel',
    'SceneTreePanel',
    'AssetBrowserPanel',
    'FileBrowserPanel',
    'InspectorPanel',
    'ScriptEditorPanel',
    'ConsolePanel',
]
