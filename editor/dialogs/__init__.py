"""
Lupine Engine Dialogs
Various dialog windows for the editor
"""

from .project_settings_dialog import ProjectSettingsDialog
from .new_project_dialog import NewProjectDialog
from .add_node_dialog import AddNodeDialog
from .add_component_dialog import AddComponentDialog
from .add_prefab_dialog import AddPrefabDialog
from .add_node_prefab_dialog import AddNodePrefabDialog
from .input_mapping_dialog import InputMappingDialog
from .theme_editor_dialog import ThemeEditorDialog
from .set_main_scene_dialog import SetMainSceneDialog
from .coming_soon_dialog import ComingSoonDialog

__all__ = [
    'ProjectSettingsDialog',
    'NewProjectDialog',
    'AddNodeDialog',
    'AddComponentDialog',
    'AddPrefabDialog',
    'AddNodePrefabDialog',
    'InputMappingDialog',
    'ThemeEditorDialog',
    'SetMainSceneDialog',
    'ComingSoonDialog',
]
