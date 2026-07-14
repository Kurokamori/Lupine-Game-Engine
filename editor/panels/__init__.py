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
from .shader_editor_panel import ShaderEditorPanel
from .console_panel import ConsolePanel
from .node_signals_panel import NodeSignalsPanel
from .interface_panel import InterfacePanel
from .animation_timeline_panel import AnimationTimelinePanel
from .blend_tree_panel import BlendTreePanel
from .profiler_panel import ProfilerPanel

__all__ = [
    'EditorPanel',
    'SceneTreePanel',
    'AssetBrowserPanel',
    'FileBrowserPanel',
    'InspectorPanel',
    'ScriptEditorPanel',
    'ShaderEditorPanel',
    'ConsolePanel',
    'NodeSignalsPanel',
    'InterfacePanel',
    'AnimationTimelinePanel',
    'BlendTreePanel',
    'ProfilerPanel',
]
