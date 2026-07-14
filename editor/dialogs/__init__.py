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
from .godot_scene_import_dialog import GodotSceneImportDialog
from .new_shader_dialog import NewShaderDialog
from .archetype_definition_dialog import ArchetypeDefinitionDialog
from .interface_definition_dialog import InterfaceDefinitionDialog
from .connect_signal_dialog import ConnectSignalDialog
from .localization_editor_dialog import LocalizationEditorDialog
from .uitheme_editor_dialog import UIThemeEditorDialog
from .plugins_dialog import PluginsDialog

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
    'GodotSceneImportDialog',
    'NewShaderDialog',
    'ArchetypeDefinitionDialog',
    'InterfaceDefinitionDialog',
    'ConnectSignalDialog',
    'LocalizationEditorDialog',
    'UIThemeEditorDialog',
    'PluginsDialog',
]
