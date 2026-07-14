"""
Lupine Engine Export Manager

Handles exporting games to various platforms using pre-built export templates.
"""

import json
import os
import shutil
import subprocess
import platform
import struct
import zipfile
from pathlib import Path
from typing import Dict, List, Optional, Callable, Any
from dataclasses import dataclass, field, asdict
from enum import Enum
from datetime import datetime

from .pack_file import PackFileWriter, append_pack_to_executable


class ExportPlatform(Enum):
    """Supported export platforms"""
    WINDOWS_X64 = "windows_x64"
    LINUX_X64 = "linux_x64"
    MACOS_ARM64 = "macos_arm64"
    MACOS_X64 = "macos_x64"
    WEB = "web"

    @property
    def display_name(self) -> str:
        names = {
            ExportPlatform.WINDOWS_X64: "Windows (x64)",
            ExportPlatform.LINUX_X64: "Linux (x64)",
            ExportPlatform.MACOS_ARM64: "macOS (Apple Silicon)",
            ExportPlatform.MACOS_X64: "macOS (Intel)",
            ExportPlatform.WEB: "Web (HTML5/WebAssembly)",
        }
        return names.get(self, self.value)

    @property
    def template_filename(self) -> str:
        """Get the template binary filename for this platform"""
        filenames = {
            ExportPlatform.WINDOWS_X64: "lupine_template_windows_x64.exe",
            ExportPlatform.LINUX_X64: "lupine_template_linux_x64",
            ExportPlatform.MACOS_ARM64: "lupine_template_macos_arm64",
            ExportPlatform.MACOS_X64: "lupine_template_macos_x64",
            ExportPlatform.WEB: "index.html",  # itch.io compatible
        }
        return filenames.get(self, "")

    @property
    def debug_template_filename(self) -> str:
        """Get the debug template binary filename for this platform.

        The debug template is the same runtime built with symbols and preserved
        function names, so a crash in an exported game yields a readable stack
        trace. It is selected when a preset enables include_debug_symbols.
        """
        filenames = {
            ExportPlatform.WINDOWS_X64: "lupine_template_windows_x64_debug.exe",
            ExportPlatform.LINUX_X64: "lupine_template_linux_x64_debug",
            ExportPlatform.MACOS_ARM64: "lupine_template_macos_arm64_debug",
            ExportPlatform.MACOS_X64: "lupine_template_macos_x64_debug",
            # Web debug artifacts are all named index.* like the release ones, so
            # they live in their own subdirectory instead of carrying a suffix.
            ExportPlatform.WEB: "debug/index.html",
        }
        return filenames.get(self, "")

    @property
    def output_extension(self) -> str:
        """Get the output file extension for this platform"""
        extensions = {
            ExportPlatform.WINDOWS_X64: ".exe",
            ExportPlatform.LINUX_X64: "",
            ExportPlatform.MACOS_ARM64: "",
            ExportPlatform.MACOS_X64: "",
            ExportPlatform.WEB: ".html",
        }
        return extensions.get(self, "")

    @property
    def platform_dir(self) -> str:
        """Get the template directory name for this platform"""
        dirs = {
            ExportPlatform.WINDOWS_X64: "windows",
            ExportPlatform.LINUX_X64: "linux",
            ExportPlatform.MACOS_ARM64: "macos",
            ExportPlatform.MACOS_X64: "macos",
            ExportPlatform.WEB: "web",
        }
        return dirs.get(self, "")


@dataclass
class ExportPreset:
    """Export configuration preset"""
    name: str = "Default"
    platform: ExportPlatform = ExportPlatform.WINDOWS_X64
    output_path: str = ""
    executable_name: str = ""

    # Pack options
    embed_pack: bool = True  # Embed .pck in executable
    compress_assets: bool = True
    compression_level: int = 9

    # Graphics backend
    graphics_backend: str = "opengl"  # opengl, vulkan, dx11, dx12, metal

    # Include options
    include_debug_symbols: bool = False
    include_editor_data: bool = False

    # Windows-specific
    windows_console: bool = False
    windows_icon_path: str = ""

    # macOS-specific
    macos_bundle_id: str = "com.lupine.game"
    macos_bundle_name: str = ""
    macos_version: str = "1.0.0"
    macos_create_dmg: bool = False

    # Web-specific
    web_canvas_resize: str = "project"  # project, adaptive, fixed
    web_focus_canvas: bool = True
    web_progressive_loading: bool = True
    web_icon_path: str = ""  # Path to favicon (png/ico), falls back to Lupine icon

    # Filtering
    exclude_patterns: List[str] = field(default_factory=list)
    include_extensions: List[str] = field(default_factory=list)

    def to_dict(self) -> Dict:
        """Convert to dictionary for serialization"""
        d = asdict(self)
        d['platform'] = self.platform.value
        return d

    @staticmethod
    def from_dict(data: Dict) -> 'ExportPreset':
        """Create from dictionary"""
        preset = ExportPreset()
        for key, value in data.items():
            if key == 'platform':
                preset.platform = ExportPlatform(value)
            elif hasattr(preset, key):
                setattr(preset, key, value)
        return preset


class ExportResult:
    """Result of an export operation"""
    def __init__(self):
        self.success: bool = False
        self.output_path: str = ""
        self.pack_path: str = ""
        self.errors: List[str] = []
        self.warnings: List[str] = []
        self.files_packed: int = 0
        self.total_size: int = 0
        self.export_time: float = 0.0


class ExportManager:
    """Manages export templates and game export process"""

    def __init__(self, templates_dir: Optional[str] = None):
        """
        Initialize the export manager

        Args:
            templates_dir: Directory containing export templates.
                          If None, uses default location.
        """
        self.templates_dir = templates_dir or self._get_default_templates_dir()
        self.presets: Dict[str, ExportPreset] = {}
        self.progress_callback: Optional[Callable[[str, float], None]] = None

        # Ensure templates directory exists
        os.makedirs(self.templates_dir, exist_ok=True)

        # Load presets
        self._load_presets()

    def _get_default_templates_dir(self) -> str:
        """Get the default templates directory"""
        # Look in several locations
        possible_paths = [
            # In build directory (highest priority - for development)
            Path(__file__).parent.parent.parent / "build" / "export_templates",
            # Adjacent to editor (for installed/packaged editor)
            Path(__file__).parent.parent.parent / "export_templates",
            # User's home directory (for downloaded templates)
            Path.home() / ".lupine" / "export_templates",
        ]

        for path in possible_paths:
            if path.exists():
                return str(path)

        # Default to build directory for development
        build_path = Path(__file__).parent.parent.parent / "build" / "export_templates"
        if not build_path.exists():
            build_path.mkdir(parents=True, exist_ok=True)
        return str(build_path)

    def _report_progress(self, message: str, progress: float = -1):
        """Report progress to callback if set"""
        if self.progress_callback:
            self.progress_callback(message, progress)
        print(f"[Export] {message}")

    def _load_presets(self):
        """Load saved presets from disk"""
        presets_file = Path(self.templates_dir) / "presets.json"
        if presets_file.exists():
            try:
                with open(presets_file, 'r') as f:
                    data = json.load(f)
                for name, preset_data in data.get('presets', {}).items():
                    self.presets[name] = ExportPreset.from_dict(preset_data)
            except Exception as e:
                print(f"Failed to load presets: {e}")

    def save_presets(self):
        """Save presets to disk"""
        presets_file = Path(self.templates_dir) / "presets.json"
        try:
            data = {
                'presets': {name: preset.to_dict() for name, preset in self.presets.items()}
            }
            with open(presets_file, 'w') as f:
                json.dump(data, f, indent=2)
        except Exception as e:
            print(f"Failed to save presets: {e}")

    def add_preset(self, preset: ExportPreset):
        """Add or update a preset"""
        self.presets[preset.name] = preset
        self.save_presets()

    def remove_preset(self, name: str):
        """Remove a preset"""
        if name in self.presets:
            del self.presets[name]
            self.save_presets()

    def get_installed_templates(self) -> Dict[ExportPlatform, Dict]:
        """Get information about installed templates"""
        templates = {}

        manifest_path = Path(self.templates_dir) / "templates.json"
        if manifest_path.exists():
            try:
                with open(manifest_path, 'r') as f:
                    manifest = json.load(f)
                for template_id, info in manifest.get('templates', {}).items():
                    platform_str = info.get('platform', '')
                    arch = info.get('architecture', '')
                    key = f"{platform_str}_{arch}"

                    for plat in ExportPlatform:
                        if plat.value == key:
                            templates[plat] = info
                            break
            except Exception as e:
                print(f"Failed to read templates manifest: {e}")

        # Also check for template files directly
        for platform in ExportPlatform:
            if platform in templates:
                continue

            template_path = Path(self.templates_dir) / platform.platform_dir / platform.template_filename
            if template_path.exists():
                templates[platform] = {
                    'platform': platform.platform_dir,
                    'binary': platform.template_filename,
                    'path': str(template_path)
                }

        return templates

    def _template_file(self, platform: ExportPlatform, debug: bool = False) -> Path:
        """Resolve the on-disk location of a template binary"""
        filename = platform.debug_template_filename if debug else platform.template_filename
        return Path(self.templates_dir) / platform.platform_dir / filename

    def is_template_available(self, platform: ExportPlatform, debug: bool = False) -> bool:
        """Check if a template is available for the given platform"""
        return self._template_file(platform, debug).exists()

    def get_template_path(self, platform: ExportPlatform,
                          debug: bool = False) -> Optional[str]:
        """Get the path to a template executable.

        When debug is requested but no debug template has been built, this returns
        None rather than silently handing back the release template - callers warn
        and fall back explicitly so the user knows the export lost its symbols.
        """
        template_path = self._template_file(platform, debug)
        if template_path.exists():
            return str(template_path)
        return None

    def resolve_template_variant(self, platform: ExportPlatform,
                                 preset: 'ExportPreset') -> tuple:
        """Decide which template variant an export should use.

        Returns (use_debug, warning), where use_debug is None when no template of
        either kind is installed. If only the other variant exists the export still
        proceeds from it with a warning: a missing optional template should degrade
        the build, not block it, but the user has to be told which one they got.
        """
        want_debug: bool = preset.include_debug_symbols

        if self.is_template_available(platform, debug=want_debug):
            return (want_debug, None)

        if not self.is_template_available(platform, debug=not want_debug):
            return (None, None)

        if want_debug:
            warning = (
                f"Debug symbols were requested but no debug export template is "
                f"installed for {platform.display_name}. Exporting from the release "
                f"template instead - the build will have no symbols or function names. "
                f"Build debug templates with the 'lupine_export_templates' target."
            )
        else:
            warning = (
                f"No release export template is installed for {platform.display_name}. "
                f"Exporting from the debug template instead - the build is larger, "
                f"unoptimized, and on Windows opens a console window."
            )
        return (not want_debug, warning)

    def export_project(self, project_path: str, preset: ExportPreset) -> ExportResult:
        """
        Export a project using the given preset

        Args:
            project_path: Path to the .lupine project file
            preset: Export configuration

        Returns:
            ExportResult with details about the export
        """
        import time
        start_time = time.time()

        result = ExportResult()

        try:
            # Validate inputs
            if not os.path.exists(project_path):
                result.errors.append(f"Project file not found: {project_path}")
                return result

            if not preset.output_path:
                result.errors.append("Output path not specified")
                return result

            # Pick the template variant (release or debug) this preset needs
            use_debug, template_warning = self.resolve_template_variant(
                preset.platform, preset
            )
            if use_debug is None:
                result.errors.append(
                    f"Export template not found for {preset.platform.display_name}. "
                    f"Please build or download the template first."
                )
                return result

            if template_warning:
                result.warnings.append(template_warning)
                self._report_progress(f"Warning: {template_warning}", -1)

            self._report_progress(
                "Starting export (debug template)..." if use_debug else "Starting export...",
                0
            )

            # Load project
            project_dir = os.path.dirname(project_path)
            with open(project_path, 'r') as f:
                project_data = json.load(f)

            # Create output directory
            output_dir = os.path.dirname(preset.output_path)
            os.makedirs(output_dir, exist_ok=True)

            # Create pack file
            self._report_progress("Packing assets...", 0.1)
            pack_result = self._create_pack_file(
                project_path, project_data, project_dir, preset
            )

            if not pack_result[0]:
                result.errors.append(f"Failed to create pack file: {pack_result[1]}")
                return result

            pack_path, files_packed, pack_size = pack_result[1], pack_result[2], pack_result[3]
            result.files_packed = files_packed
            result.total_size = pack_size
            result.pack_path = pack_path

            # Create final executable
            self._report_progress("Creating executable...", 0.7)

            if preset.platform == ExportPlatform.WEB:
                # Web export
                export_success = self._export_web(
                    project_data, pack_path, preset, output_dir, use_debug
                )
            else:
                # Desktop export
                export_success = self._export_desktop(
                    project_data, pack_path, preset, project_dir, use_debug
                )

            if not export_success[0]:
                result.errors.append(export_success[1])
                return result

            result.output_path = export_success[1]

            # Cleanup temp pack file if embedded
            if preset.embed_pack and os.path.exists(pack_path) and pack_path != result.pack_path:
                try:
                    os.remove(pack_path)
                except:
                    pass

            self._report_progress("Export complete!", 1.0)

            result.success = True
            result.export_time = time.time() - start_time

        except Exception as e:
            result.errors.append(f"Export failed: {str(e)}")
            import traceback
            traceback.print_exc()

        return result

    # Resource kinds the runtime loads through res:// but that may live in any project
    # directory. The scenes/assets/scripts passes only cover three well-known folders,
    # so anything a project keeps elsewhere (prefab/, interfaces/, data/, ...) has to be
    # swept for explicitly or it is simply absent from the exported game.
    PROJECT_RESOURCE_EXTENSIONS = (
        '.archetype',    # archetype definitions
        '.ares',         # archetype instances
        '.prefab',       # prefabs instanced by scenes and scripts
        '.interface',    # interface declarations read by InterfaceRegistry
        '.uitheme',
        '.palette',
        '.loctable',
        '.json',         # data tables (e.g. data/vn/*.vn.json), configs
    )

    def _add_project_resource_files(self, writer, project_dir: str, preset: ExportPreset) -> int:
        """Pack every runtime resource file found anywhere in the project, mirroring its
        project-relative path so res:// lookups resolve at runtime."""
        count = 0
        exclude_patterns = (preset.exclude_patterns or []) + ['__pycache__', '.git']
        project_root = os.path.abspath(project_dir)

        for root, dirs, files in os.walk(project_root):
            dirs[:] = [d for d in dirs if not any(p in d for p in exclude_patterns)]
            for name in files:
                if not name.lower().endswith(self.PROJECT_RESOURCE_EXTENSIONS):
                    continue
                # Dotfiles are editor bookkeeping (.lupine_layout.json and friends), never
                # something the running game loads.
                if name.startswith('.'):
                    continue
                abs_path = os.path.join(root, name)
                rel_path = os.path.relpath(abs_path, project_root).replace('\\', '/')
                if any(p in rel_path for p in exclude_patterns):
                    continue
                if writer.add_file(rel_path, abs_path):
                    count += 1

        return count

    def _create_pack_file(self, project_path: str, project_data: Dict,
                          project_dir: str, preset: ExportPreset) -> tuple:
        """Create the pack file with all game assets"""
        try:
            writer = PackFileWriter()
            writer.set_compression(preset.compress_assets, preset.compression_level)

            # Add project file (modified for export)
            export_project = self._create_export_project(project_data, preset)
            writer.add_string("project.lupine", json.dumps(export_project, indent=2))

            # Get directories from project
            scenes_dir = project_data.get('structure', {}).get('scenes_directory', 'scenes')
            assets_dir = project_data.get('structure', {}).get('assets_directory', 'assets')
            scripts_dir = project_data.get('structure', {}).get('scripts_directory', 'scripts')

            # Add scenes
            scenes_path = os.path.join(project_dir, scenes_dir)
            if os.path.exists(scenes_path):
                count = writer.add_directory(
                    scenes_dir, scenes_path,
                    extensions=['.scene', '.json'],
                    exclude_patterns=preset.exclude_patterns
                )
                self._report_progress(f"Added {count} scene files", 0.3)

            # Add assets
            assets_path = os.path.join(project_dir, assets_dir)
            if os.path.exists(assets_path):
                count = writer.add_directory(
                    assets_dir, assets_path,
                    exclude_patterns=preset.exclude_patterns + ['__pycache__', '.git']
                )
                self._report_progress(f"Added {count} asset files", 0.5)

            # Add scripts
            scripts_path = os.path.join(project_dir, scripts_dir)
            if os.path.exists(scripts_path):
                count = writer.add_directory(
                    scripts_dir, scripts_path,
                    extensions=['.lua', '.rb', '.py', '.json'],
                    exclude_patterns=preset.exclude_patterns + ['__pycache__']
                )
                self._report_progress(f"Added {count} script files", 0.6)

            # Add archetypes, prefabs, interfaces and data tables found anywhere in the
            # project. The editor resolves these against the whole project tree, but the
            # passes above only cover scenes/assets/scripts, so a resource kept in any
            # other directory is missing at runtime.
            resource_count = self._add_project_resource_files(writer, project_dir, preset)
            if resource_count:
                self._report_progress(f"Added {resource_count} project resource files", 0.62)

            # Add input map if exists
            input_map_path = os.path.join(project_dir, 'input_map.json')
            if os.path.exists(input_map_path):
                writer.add_file('input_map.json', input_map_path)

            # Add globals config so autoload singletons and global variables exist in
            # the exported game (the engine reads globals.json at project load).
            globals_path = os.path.join(project_dir, 'globals.json')
            if os.path.exists(globals_path):
                writer.add_file('globals.json', globals_path)

            # Add localization config + tables so localized text resolves in the
            # exported game (the engine reads localization.json + the tables dir).
            localization_json = os.path.join(project_dir, 'localization.json')
            if os.path.exists(localization_json):
                writer.add_file('localization.json', localization_json)
            tables_dir = project_data.get('localization', {}).get('tables_dir', 'localization')
            tables_path = os.path.join(project_dir, tables_dir)
            if os.path.isdir(tables_path):
                writer.add_directory(
                    tables_dir, tables_path,
                    extensions=['.loctable', '.csv', '.json'],
                    exclude_patterns=preset.exclude_patterns
                )

            # Add the default UI theme + palette referenced by the project so the
            # default.uitheme convention resolves at runtime. These usually live
            # at the project root (outside scenes/assets/scripts).
            default_theme = project_data.get('rendering', {}).get('default_theme', '')
            theme_candidates = []
            if default_theme.startswith('res://'):
                theme_candidates.append(default_theme[len('res://'):])
            theme_candidates.extend(['default.uitheme', 'default.palette'])
            for rel in theme_candidates:
                rel = rel.replace('\\', '/')
                src = os.path.join(project_dir, rel)
                if os.path.isfile(src):
                    writer.add_file(rel, src)

            # Add the project/game icon so the runtime can set the window icon.
            # It usually lives at the project root (outside scenes/assets/scripts).
            icon_rel = project_data.get('project', {}).get('icon', '')
            if icon_rel:
                icon_rel = icon_rel.replace('\\', '/')
                if icon_rel.startswith('res://'):
                    icon_rel = icon_rel[len('res://'):]
                icon_src = os.path.join(project_dir, icon_rel)
                if os.path.isfile(icon_src):
                    writer.add_file(icon_rel, icon_src)

            # Add UUID mappings from AssetDatabase for asset resolution at runtime
            try:
                import lupine_engine
                asset_db = lupine_engine.AssetDatabase.get_instance()
                if asset_db.is_initialized():
                    uuid_mappings_json = asset_db.export_uuid_mappings()
                    if uuid_mappings_json:
                        writer.add_string('__uuid_mappings__.json', uuid_mappings_json)
                        self._report_progress("Added UUID mappings for asset resolution", 0.65)
            except Exception as e:
                # Non-fatal - UUID mappings are optional for backwards compatibility
                print(f"Warning: Could not export UUID mappings: {e}")

            # Write pack file
            if preset.embed_pack:
                # Write to temp location
                import tempfile
                pack_path = os.path.join(tempfile.gettempdir(), 'lupine_export_temp.pck')
            else:
                # Write next to executable
                output_dir = os.path.dirname(preset.output_path)
                exe_name = os.path.splitext(os.path.basename(preset.output_path))[0]
                pack_path = os.path.join(output_dir, f"{exe_name}.pck")

            if not writer.write(pack_path):
                return (False, "Failed to write pack file", 0, 0)

            return (True, pack_path, writer.get_file_count(), writer.get_total_size())

        except Exception as e:
            return (False, str(e), 0, 0)

    def _create_export_project(self, project_data: Dict, preset: ExportPreset) -> Dict:
        """Create the project file embedded in the export pack.

        The exported project.lupine carries the COMPLETE, unified project
        settings (the same nested schema written by the editor and consumed by
        Project::LoadFromString in Project.cpp): project, window, graphics,
        physics, audio, rendering (incl. clear_color), editor, input_map,
        splash_screens and localization. The export runtime parses this into a
        full core::Project so every setting is honored in the shipped game.

        Only export-specific metadata (the graphics backend chosen for this
        preset) is overlaid on top of the project's own settings.
        """
        import copy

        export_data = copy.deepcopy(project_data)

        # Guarantee the sections the runtime expects always exist, even for
        # older project files written before these settings were added.
        export_data.setdefault('project', {}).setdefault('name', 'Lupine Game')
        window = export_data.setdefault('window', {})
        window.setdefault('width', 1280)
        window.setdefault('height', 720)
        window.setdefault('resizable', True)
        export_data.setdefault('graphics', {})

        # Export metadata: the graphics backend is a per-preset choice, not a
        # project setting, so it is layered on here.
        export_section = export_data.setdefault('export', {})
        export_section['graphics_backend'] = preset.graphics_backend

        return export_data

    def _export_desktop(self, project_data: Dict, pack_path: str,
                        preset: ExportPreset, project_dir: str = "",
                        use_debug: bool = False) -> tuple:
        """Export for desktop platforms (Windows, Linux, macOS)"""
        try:
            template_path = self.get_template_path(preset.platform, debug=use_debug)
            if not template_path:
                return (False, "Template not found")

            # Determine output path
            output_path = preset.output_path
            if not output_path.endswith(preset.platform.output_extension):
                output_path += preset.platform.output_extension

            # Ensure output directory exists
            os.makedirs(os.path.dirname(output_path), exist_ok=True)

            # Start from a plain copy of the template so the Windows resource
            # update (icon embedding) can run before any pack data is appended.
            # The Win32 EndUpdateResource API rewrites the PE image and discards
            # overlay data appended past the final section, so the embedded pack
            # must be appended AFTER the icon is embedded.
            shutil.copy2(template_path, output_path)

            # Embed the executable icon on Windows. Uses the preset's explicit
            # icon when provided, otherwise the project's own icon.
            if preset.platform == ExportPlatform.WINDOWS_X64:
                icon_source = self._resolve_desktop_icon(project_data, preset, project_dir)
                if icon_source:
                    if not self._embed_windows_icon(output_path, icon_source):
                        self._report_progress(
                            "Warning: could not embed executable icon", -1
                        )

            if preset.embed_pack:
                # Append the pack as overlay data at the very end of the exe,
                # after the icon resources have been written.
                if not append_pack_to_executable(output_path, pack_path, output_path):
                    return (False, "Failed to embed pack in executable")
            # When not embedding, the pack file is already placed alongside the
            # output by _create_pack_file.

            if use_debug:
                self._copy_debug_symbols(template_path, output_path)

            # Set executable permissions on Unix
            if preset.platform in [ExportPlatform.LINUX_X64, ExportPlatform.MACOS_ARM64, ExportPlatform.MACOS_X64]:
                os.chmod(output_path, 0o755)

            # macOS bundle creation
            if preset.platform in [ExportPlatform.MACOS_ARM64, ExportPlatform.MACOS_X64]:
                if preset.macos_bundle_name:
                    bundle_path = self._create_macos_bundle(
                        output_path, pack_path, project_data, preset
                    )
                    if bundle_path:
                        output_path = bundle_path

            return (True, output_path)

        except Exception as e:
            return (False, str(e))

    def _copy_debug_symbols(self, template_path: str, output_path: str) -> bool:
        """Ship the debug template's side-car symbol file next to the exported game.

        Only Windows has one: MSVC keeps debug info in a .pdb, while the Linux and
        macOS debug templates carry their symbols inside the binary itself, so there
        is nothing to copy there.

        The PDB keeps its original filename on purpose. A PE records the name of its
        PDB in the debug directory at link time, and that is the name a debugger goes
        looking for - renaming the file to match the exported executable would make
        it undiscoverable.
        """
        template_pdb = Path(template_path).with_suffix('.pdb')
        if not template_pdb.exists():
            return False

        dest = Path(output_path).parent / template_pdb.name
        try:
            shutil.copy2(template_pdb, dest)
            self._report_progress(f"Copied debug symbols: {template_pdb.name}", -1)
            return True
        except OSError as e:
            self._report_progress(f"Warning: could not copy debug symbols: {e}", -1)
            return False

    def _resolve_desktop_icon(self, project_data: Dict, preset: ExportPreset,
                              project_dir: str) -> Optional[str]:
        """Resolve the image to use as the executable icon.

        Priority: explicit preset icon, then the project's own icon, then the
        engine's default Lupine icon.
        """
        if preset.windows_icon_path and os.path.exists(preset.windows_icon_path):
            return preset.windows_icon_path

        icon_rel = project_data.get('project', {}).get('icon', '')
        if icon_rel and project_dir:
            icon_rel = icon_rel.replace('\\', '/')
            if icon_rel.startswith('res://'):
                icon_rel = icon_rel[len('res://'):]
            icon_src = os.path.join(project_dir, icon_rel)
            if os.path.isfile(icon_src):
                return icon_src

        default_icon = Path(__file__).parent.parent / "resources" / "icon.png"
        if default_icon.exists():
            return str(default_icon)

        return None

    def _embed_windows_icon(self, exe_path: str, icon_source: str) -> bool:
        """Embed an icon into a Windows PE executable.

        Builds a multi-resolution icon from the source image and writes the
        RT_ICON / RT_GROUP_ICON resources via the Win32 resource-update API.
        Only functional when the export is run on Windows.
        """
        if platform.system().lower() != 'windows':
            return False

        try:
            import ctypes
            from ctypes import wintypes
            from PIL import Image
            import io

            img = Image.open(icon_source)
            if img.mode != 'RGBA':
                img = img.convert('RGBA')

            max_dim = max(img.size)
            candidate_sizes = [(16, 16), (24, 24), (32, 32), (48, 48),
                               (64, 64), (128, 128), (256, 256)]
            ico_sizes = [s for s in candidate_sizes if s[0] <= max_dim]
            if not ico_sizes:
                ico_sizes = [img.size]

            ico_buf = io.BytesIO()
            img.save(ico_buf, format='ICO', sizes=ico_sizes)
            ico_data = ico_buf.getvalue()

            reserved, itype, count = struct.unpack('<HHH', ico_data[:6])
            if count == 0:
                return False

            images = []
            entry_offset = 6
            for _ in range(count):
                (b_width, b_height, b_color_count, b_reserved,
                 w_planes, w_bit_count, dw_bytes, dw_offset) = struct.unpack(
                    '<BBBBHHII', ico_data[entry_offset:entry_offset + 16])
                images.append({
                    'width': b_width,
                    'height': b_height,
                    'color_count': b_color_count,
                    'planes': w_planes,
                    'bit_count': w_bit_count,
                    'data': ico_data[dw_offset:dw_offset + dw_bytes],
                })
                entry_offset += 16

            first_icon_id = 1
            group_icon_id = 1
            group = struct.pack('<HHH', 0, 1, count)
            for i, im in enumerate(images):
                group += struct.pack(
                    '<BBBBHHIH',
                    im['width'] & 0xFF, im['height'] & 0xFF, im['color_count'],
                    0, im['planes'], im['bit_count'], len(im['data']),
                    first_icon_id + i)

            RT_ICON = 3
            RT_GROUP_ICON = 14
            LANG_NEUTRAL = 0

            kernel32 = ctypes.WinDLL('kernel32', use_last_error=True)
            begin_update = kernel32.BeginUpdateResourceW
            begin_update.argtypes = [wintypes.LPCWSTR, wintypes.BOOL]
            begin_update.restype = wintypes.HANDLE
            update_resource = kernel32.UpdateResourceW
            update_resource.argtypes = [wintypes.HANDLE, wintypes.LPCWSTR,
                                        wintypes.LPCWSTR, wintypes.WORD,
                                        wintypes.LPVOID, wintypes.DWORD]
            update_resource.restype = wintypes.BOOL
            end_update = kernel32.EndUpdateResourceW
            end_update.argtypes = [wintypes.HANDLE, wintypes.BOOL]
            end_update.restype = wintypes.BOOL

            def make_int_resource(value: int):
                return ctypes.cast(ctypes.c_void_p(value), wintypes.LPCWSTR)

            handle = begin_update(exe_path, False)
            if not handle:
                return False

            for i, im in enumerate(images):
                data = im['data']
                if not update_resource(handle, make_int_resource(RT_ICON),
                                       make_int_resource(first_icon_id + i),
                                       LANG_NEUTRAL, data, len(data)):
                    end_update(handle, True)
                    return False

            if not update_resource(handle, make_int_resource(RT_GROUP_ICON),
                                   make_int_resource(group_icon_id),
                                   LANG_NEUTRAL, group, len(group)):
                end_update(handle, True)
                return False

            return bool(end_update(handle, False))

        except Exception as e:
            print(f"Failed to embed Windows icon: {e}")
            return False

    def _create_macos_bundle(self, exe_path: str, pack_path: str,
                              project_data: Dict, preset: ExportPreset) -> Optional[str]:
        """Create a macOS .app bundle"""
        try:
            bundle_name = preset.macos_bundle_name or project_data.get('project', {}).get('name', 'Game')
            bundle_path = os.path.join(os.path.dirname(exe_path), f"{bundle_name}.app")

            # Create bundle structure
            contents_dir = os.path.join(bundle_path, "Contents")
            macos_dir = os.path.join(contents_dir, "MacOS")
            resources_dir = os.path.join(contents_dir, "Resources")

            os.makedirs(macos_dir, exist_ok=True)
            os.makedirs(resources_dir, exist_ok=True)

            # Move executable
            exe_name = os.path.basename(exe_path)
            shutil.move(exe_path, os.path.join(macos_dir, exe_name))

            # Copy pack file if not embedded
            if not preset.embed_pack and os.path.exists(pack_path):
                shutil.copy2(pack_path, os.path.join(resources_dir, "game.pck"))

            # Create Info.plist
            info_plist = f"""<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleDevelopmentRegion</key>
    <string>en</string>
    <key>CFBundleExecutable</key>
    <string>{exe_name}</string>
    <key>CFBundleIdentifier</key>
    <string>{preset.macos_bundle_id}</string>
    <key>CFBundleInfoDictionaryVersion</key>
    <string>6.0</string>
    <key>CFBundleName</key>
    <string>{bundle_name}</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>{preset.macos_version}</string>
    <key>CFBundleVersion</key>
    <string>{preset.macos_version}</string>
    <key>LSMinimumSystemVersion</key>
    <string>10.13</string>
    <key>NSHighResolutionCapable</key>
    <true/>
    <key>NSPrincipalClass</key>
    <string>NSApplication</string>
</dict>
</plist>
"""
            with open(os.path.join(contents_dir, "Info.plist"), 'w') as f:
                f.write(info_plist)

            # Remove original exe path
            if os.path.exists(exe_path):
                os.remove(exe_path)

            return bundle_path

        except Exception as e:
            print(f"Failed to create macOS bundle: {e}")
            return None

    def _export_web(self, project_data: Dict, pack_path: str,
                    preset: ExportPreset, output_dir: str,
                    use_debug: bool = False) -> tuple:
        """Export for web platform"""
        try:
            # The debug web template is a full second Emscripten build (function
            # names preserved in the wasm name section, assertions on) staged in
            # its own subdirectory, because every one of its files is called
            # index.* exactly like the release template's.
            release_dir = Path(self.templates_dir) / "web"
            template_dir = release_dir / "debug" if use_debug else release_dir
            if not template_dir.exists():
                return (False, "Web template not found")

            # Create output directory
            os.makedirs(output_dir, exist_ok=True)

            # Get project info
            project_name = project_data.get('project', {}).get('name', 'Lupine Game')
            window_width = project_data.get('window', {}).get('width', 1280)
            window_height = project_data.get('window', {}).get('height', 720)

            # Copy and customize index.html
            shell_path = template_dir / "index.html"
            if shell_path.exists():
                with open(shell_path, 'r') as f:
                    html_content = f.read()
            else:
                # Use built-in shell
                shell_source = Path(__file__).parent.parent.parent / "runtime" / "web" / "shell.html"
                if shell_source.exists():
                    with open(shell_source, 'r') as f:
                        html_content = f.read()
                else:
                    return (False, f"Web template not found at {shell_path} or {shell_source}")

            # Replace placeholders
            html_content = html_content.replace('{{{ LUPINE_GAME_TITLE }}}', project_name)
            html_content = html_content.replace('{{{ LUPINE_GAME_WIDTH }}}', str(window_width))
            html_content = html_content.replace('{{{ LUPINE_GAME_HEIGHT }}}', str(window_height))

            # Write index.html
            index_path = os.path.join(output_dir, "index.html")
            with open(index_path, 'w') as f:
                f.write(html_content)

            # Copy WASM, JS, and data files from template. The debug template also
            # emits index.wasm.symbols (--emit-symbol-map), which browser tooling
            # uses to turn wasm frame indices back into C++ function names.
            template_extensions = ['.wasm', '.js', '.data']
            if use_debug:
                template_extensions.append('.symbols')
            for ext in template_extensions:
                for template_file in template_dir.glob(f"*{ext}"):
                    shutil.copy2(template_file, output_dir)

            # Copy pack file as game.pck
            pack_dest = os.path.join(output_dir, "game.pck")
            shutil.copy2(pack_path, pack_dest)

            # Handle favicon
            favicon_dest = os.path.join(output_dir, "favicon.ico")
            favicon_copied = False

            # Priority 1: User-specified icon
            if preset.web_icon_path and os.path.exists(preset.web_icon_path):
                shutil.copy2(preset.web_icon_path, favicon_dest)
                favicon_copied = True

            # Priority 2: Template favicon (ico or png). The favicon is installed
            # once per platform, in the release template directory, so a debug
            # export has to look one level up as well.
            if not favicon_copied:
                template_favicon = template_dir / "favicon.ico"
                if not template_favicon.exists():
                    template_favicon = release_dir / "favicon.ico"

                if template_favicon.exists():
                    shutil.copy2(template_favicon, favicon_dest)
                    favicon_copied = True
                else:
                    # Try PNG favicon from template
                    template_favicon_png = template_dir / "favicon.png"
                    if not template_favicon_png.exists():
                        template_favicon_png = release_dir / "favicon.png"

                    if template_favicon_png.exists():
                        favicon_png_dest = os.path.join(output_dir, "favicon.png")
                        shutil.copy2(template_favicon_png, favicon_png_dest)
                        favicon_copied = True

            # Priority 3: Default Lupine icon from editor resources
            if not favicon_copied:
                default_icon = Path(__file__).parent.parent / "resources" / "favicon.ico"
                if default_icon.exists():
                    shutil.copy2(default_icon, favicon_dest)
                    favicon_copied = True
                else:
                    # Try PNG icon as fallback (browsers can handle PNG favicons)
                    png_icon = Path(__file__).parent.parent / "resources" / "icon.png"
                    if png_icon.exists():
                        # Copy as favicon.png and update HTML to reference it
                        favicon_png_dest = os.path.join(output_dir, "favicon.png")
                        shutil.copy2(png_icon, favicon_png_dest)
                        favicon_copied = True

            return (True, index_path)

        except Exception as e:
            return (False, str(e))

    def create_zip_distribution(self, export_result: ExportResult,
                                 output_zip: str) -> bool:
        """
        Create a ZIP archive of the exported game

        Args:
            export_result: Result from export_project
            output_zip: Path for the output ZIP file

        Returns:
            True if successful
        """
        try:
            if not export_result.success:
                return False

            output_path = export_result.output_path
            output_dir = os.path.dirname(output_path)

            with zipfile.ZipFile(output_zip, 'w', zipfile.ZIP_DEFLATED) as zf:
                # Add all files in output directory
                for root, dirs, files in os.walk(output_dir):
                    for file in files:
                        file_path = os.path.join(root, file)
                        arc_name = os.path.relpath(file_path, output_dir)
                        zf.write(file_path, arc_name)

            return True

        except Exception as e:
            print(f"Failed to create ZIP: {e}")
            return False


# Utility functions

def get_current_platform() -> ExportPlatform:
    """Get the export platform for the current system"""
    system = platform.system().lower()
    machine = platform.machine().lower()

    if system == 'windows':
        return ExportPlatform.WINDOWS_X64
    elif system == 'linux':
        return ExportPlatform.LINUX_X64
    elif system == 'darwin':
        if machine == 'arm64':
            return ExportPlatform.MACOS_ARM64
        else:
            return ExportPlatform.MACOS_X64
    else:
        return ExportPlatform.LINUX_X64


def create_default_presets() -> List[ExportPreset]:
    """Create default export presets for all platforms"""
    presets = []

    for platform in ExportPlatform:
        preset = ExportPreset(
            name=f"Default {platform.display_name}",
            platform=platform
        )

        # Platform-specific defaults
        if platform == ExportPlatform.WINDOWS_X64:
            preset.graphics_backend = "dx11"
        elif platform == ExportPlatform.WEB:
            preset.graphics_backend = "webgl"
            preset.embed_pack = False  # Web can't embed
        elif platform in [ExportPlatform.MACOS_ARM64, ExportPlatform.MACOS_X64]:
            preset.graphics_backend = "metal"

        presets.append(preset)

    return presets
