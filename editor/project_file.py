"""
Lupine Engine Project File Handler
Manages .lupine project files and project data structures
"""

import json
import os
import shutil
from pathlib import Path
from typing import Optional, Dict
from datetime import datetime


class ProjectData:
    """Represents a Lupine project"""
    
    def __init__(self):
        self.name: str = ""
        self.path: str = ""
        self.version: str = "1.0.0"
        self.creator: str = ""
        self.created_date: str = ""
        self.last_modified: str = ""
        self.icon_path: str = ""
        self.main_scene: str = ""
        
        # Project structure
        self.scenes_dir: str = "scenes"
        self.assets_dir: str = "assets"
        self.scripts_dir: str = "scripts"
        
        # Window settings
        self.window_width: int = 1280
        self.window_height: int = 720
        self.fullscreen: bool = False
        self.vsync: bool = True
        self.target_fps: int = 60
        self.window_resizable: bool = True
        self.window_borderless: bool = False

        # Actual window-size override. window_width/window_height are the design
        # resolution (logical canvas); when window_size_override is enabled the OS
        # window opens at window_size_override_width x window_size_override_height
        # instead, and the design canvas is scaled onto it.
        self.window_size_override: bool = False
        self.window_size_override_width: int = 1280
        self.window_size_override_height: int = 720

        # Graphics settings
        self.scale_mode: str = "letterbox"  # letterbox, stretch, crop, ignore
        self.texture_filtering: str = "bilinear"  # cubic, bilinear, nearest

        # Background clear color (RGBA, 0.0 - 1.0), used by the runtime cameras.
        self.clear_color: list = [0.1, 0.1, 0.1, 1.0]

        # Physics settings (gravity matches the engine's Y-up convention; 2D is
        # in pixels/s^2, 3D in meters/s^2).
        self.physics_tick_rate: float = 60.0
        self.gravity_2d: list = [0.0, -980.0]
        self.gravity_3d: list = [0.0, -9.8, 0.0]

        # Networking / multiplayer defaults (used by scripts that read project
        # defaults; games may override per-session in Network.start_*).
        self.networking_transport: str = "enet"     # enet | websocket | loopback
        self.networking_default_port: int = 7777
        self.networking_max_peers: int = 32
        self.networking_tick_rate: float = 30.0      # snapshot send rate (Hz)
        self.networking_interp_delay_ms: float = 100.0
        self.networking_keyframe_interval_ms: float = 1000.0  # reliable full-state cadence
        self.networking_ping_interval_seconds: float = 1.0    # RTT probe cadence
        self.networking_interest_radius: float = 0.0          # area-of-interest cull (0 = off)
        self.networking_protocol_version: int = 1
        self.networking_game_id: str = "lupine-game"
        self.networking_enable_lan_discovery: bool = False
        self.networking_discovery_port: int = 7779
        self.networking_server_name: str = "Lupine Server"
        self.networking_enable_prediction: bool = True        # client-side prediction
        self.networking_input_redundancy: int = 3             # input commands per packet
        self.networking_auto_reconnect: bool = False          # client retries on drop
        self.networking_reconnect_attempts: int = 3
        self.networking_reconnect_delay_seconds: float = 2.0
        self.networking_resume_timeout_seconds: float = 0.0   # server slot-hold (0 = off)
        self.networking_max_messages_per_second: int = 0      # anti-flood (0 = off)
        self.networking_max_bytes_per_second: int = 0

        # Audio bus volumes (0.0 - 1.0).
        self.master_volume: float = 1.0
        self.music_volume: float = 1.0
        self.sfx_volume: float = 1.0

        # Full audio mixer state (buses + DSP effect chains) as serialized by the
        # engine AudioManager. Authored via the Audio Mixer panel. Empty until the
        # mixer is used.
        self.audio_buses_state: dict = {}

        # Editor settings
        self.save_on_play: bool = True
        self.max_undo_steps: int = 100

        # Runtime launch settings (used by the editor's Play buttons). The
        # instance count lets multiplayer/networking be tested by launching
        # several isolated copies of the game at once; each instance can be
        # given its own user:// directory so save data, configs and networking
        # state never collide. Extra runtime args are forwarded to every
        # instance verbatim (readable from game scripts via get_cmdline_args()).
        self.runtime_instance_count: int = 1
        self.runtime_unique_user_dir: bool = True
        self.runtime_args: str = ""

        # Input mapping
        self.input_map: Dict = {"actions": [], "axes": []}

        # Collision layer names (32 each for 2D and 3D)
        self.collision_layers_2d: list = [f"Layer {i+1}" for i in range(32)]
        self.collision_layers_3d: list = [f"Layer {i+1}" for i in range(32)]

        # Splash screen settings
        self.splash_screens_enabled: bool = True
        self.splash_screens_allow_skip: bool = True
        self.splash_screen_entries: list = []  # List of {"image_path", "fade_in_duration", "hold_duration", "fade_out_duration"}

        # Default font setting (res:// path to TTF/OTF file, empty = use engine default)
        self.default_font: str = ""

        # Default UI theme (res:// path to a .uitheme; empty = the res://default.uitheme convention)
        self.default_theme: str = ""

        # Localization configuration (mirrors C++ LocalizationConfig; the engine
        # reads this from localization.json written alongside the project).
        self.localization: Dict = {
            "enabled": True,
            "default_locale": "en",
            "fallback_locale": "en",
            "locales": ["en"],
            "tables_dir": "localization",
            "csv_mode": False,
            "pseudolocalization": False,
        }

    def to_dict(self) -> Dict:
        """Convert project data to dictionary for serialization"""
        return {
            "project": {
                "name": self.name,
                "version": self.version,
                "creator": self.creator,
                "created_date": self.created_date,
                "last_modified": self.last_modified,
                "icon": self.icon_path,
                "main_scene": self.main_scene,
            },
            "structure": {
                "scenes_directory": self.scenes_dir,
                "assets_directory": self.assets_dir,
                "scripts_directory": self.scripts_dir,
            },
            "window": {
                "width": self.window_width,
                "height": self.window_height,
                "fullscreen": self.fullscreen,
                "vsync": self.vsync,
                "target_fps": self.target_fps,
                "resizable": self.window_resizable,
                "borderless": self.window_borderless,
                "size_override": self.window_size_override,
                "override_width": self.window_size_override_width,
                "override_height": self.window_size_override_height,
            },
            "graphics": {
                "scale_mode": self.scale_mode,
                "texture_filtering": self.texture_filtering,
            },
            "editor": {
                "save_on_play": self.save_on_play,
                "max_undo_steps": self.max_undo_steps,
            },
            "runtime": {
                "instance_count": self.runtime_instance_count,
                "unique_user_dir": self.runtime_unique_user_dir,
                "args": self.runtime_args,
            },
            "input_map": self.input_map,
            "physics": {
                "physics_tick_rate": self.physics_tick_rate,
                "gravity_2d": self.gravity_2d,
                "gravity_3d": self.gravity_3d,
                "collision_layers_2d": self.collision_layers_2d,
                "collision_layers_3d": self.collision_layers_3d,
            },
            "audio": {
                "master_volume": self.master_volume,
                "music_volume": self.music_volume,
                "sfx_volume": self.sfx_volume,
                "buses_state": self.audio_buses_state,
            },
            "networking": {
                "transport": self.networking_transport,
                "default_port": self.networking_default_port,
                "max_peers": self.networking_max_peers,
                "tick_rate": self.networking_tick_rate,
                "interp_delay_ms": self.networking_interp_delay_ms,
                "keyframe_interval_ms": self.networking_keyframe_interval_ms,
                "ping_interval_seconds": self.networking_ping_interval_seconds,
                "interest_radius": self.networking_interest_radius,
                "protocol_version": self.networking_protocol_version,
                "game_id": self.networking_game_id,
                "enable_lan_discovery": self.networking_enable_lan_discovery,
                "discovery_port": self.networking_discovery_port,
                "server_name": self.networking_server_name,
                "enable_prediction": self.networking_enable_prediction,
                "input_redundancy": self.networking_input_redundancy,
                "auto_reconnect": self.networking_auto_reconnect,
                "reconnect_attempts": self.networking_reconnect_attempts,
                "reconnect_delay_seconds": self.networking_reconnect_delay_seconds,
                "resume_timeout_seconds": self.networking_resume_timeout_seconds,
                "max_messages_per_second": self.networking_max_messages_per_second,
                "max_bytes_per_second": self.networking_max_bytes_per_second,
            },
            "splash_screens": {
                "enabled": self.splash_screens_enabled,
                "allow_skip": self.splash_screens_allow_skip,
                "entries": self.splash_screen_entries,
            },
            "rendering": {
                "default_font": self.default_font,
                "default_theme": self.default_theme,
                "clear_color": self.clear_color,
            },
            "localization": self.localization,
        }
    
    @staticmethod
    def from_dict(data: Dict, project_path: str = "") -> 'ProjectData':
        """Load project data from dictionary"""
        project = ProjectData()
        project.path = project_path
        
        if "project" in data:
            proj_data = data["project"]
            project.name = proj_data.get("name", "")
            project.version = proj_data.get("version", "1.0.0")
            project.creator = proj_data.get("creator", "")
            project.created_date = proj_data.get("created_date", "")
            project.last_modified = proj_data.get("last_modified", "")
            project.icon_path = proj_data.get("icon", "")
            project.main_scene = proj_data.get("main_scene", "")
        
        if "structure" in data:
            struct_data = data["structure"]
            project.scenes_dir = struct_data.get("scenes_directory", "scenes")
            project.assets_dir = struct_data.get("assets_directory", "assets")
            project.scripts_dir = struct_data.get("scripts_directory", "scripts")
        
        if "window" in data:
            win_data = data["window"]
            project.window_width = win_data.get("width", 1280)
            project.window_height = win_data.get("height", 720)
            project.fullscreen = win_data.get("fullscreen", False)
            project.vsync = win_data.get("vsync", True)
            project.target_fps = win_data.get("target_fps", 60)
            project.window_resizable = win_data.get("resizable", True)
            project.window_borderless = win_data.get("borderless", False)
            project.window_size_override = win_data.get("size_override", False)
            project.window_size_override_width = win_data.get("override_width", 1280)
            project.window_size_override_height = win_data.get("override_height", 720)

        if "graphics" in data:
            graphics_data = data["graphics"]
            project.scale_mode = graphics_data.get("scale_mode", "letterbox")
            project.texture_filtering = graphics_data.get("texture_filtering", "bilinear")

        if "editor" in data:
            editor_data = data["editor"]
            project.save_on_play = editor_data.get("save_on_play", True)
            project.max_undo_steps = editor_data.get("max_undo_steps", 100)

        if "runtime" in data:
            runtime_data = data["runtime"]
            project.runtime_instance_count = int(runtime_data.get("instance_count", 1))
            project.runtime_unique_user_dir = bool(runtime_data.get("unique_user_dir", True))
            project.runtime_args = str(runtime_data.get("args", ""))

        if "input_map" in data:
            project.input_map = data["input_map"]

        if "physics" in data:
            physics_data = data["physics"]
            project.physics_tick_rate = physics_data.get("physics_tick_rate", 60.0)
            project.gravity_2d = physics_data.get("gravity_2d", [0.0, -980.0])
            project.gravity_3d = physics_data.get("gravity_3d", [0.0, -9.8, 0.0])
            if "collision_layers_2d" in physics_data:
                project.collision_layers_2d = physics_data["collision_layers_2d"]
            if "collision_layers_3d" in physics_data:
                project.collision_layers_3d = physics_data["collision_layers_3d"]

        if "audio" in data:
            audio_data = data["audio"]
            project.master_volume = audio_data.get("master_volume", 1.0)
            project.music_volume = audio_data.get("music_volume", 1.0)
            project.sfx_volume = audio_data.get("sfx_volume", 1.0)
            project.audio_buses_state = audio_data.get("buses_state", {}) or {}

        if "networking" in data:
            net_data = data["networking"]
            project.networking_transport = net_data.get("transport", "enet")
            project.networking_default_port = net_data.get("default_port", 7777)
            project.networking_max_peers = net_data.get("max_peers", 32)
            project.networking_tick_rate = net_data.get("tick_rate", 30.0)
            project.networking_interp_delay_ms = net_data.get("interp_delay_ms", 100.0)
            project.networking_keyframe_interval_ms = net_data.get("keyframe_interval_ms", 1000.0)
            project.networking_ping_interval_seconds = net_data.get("ping_interval_seconds", 1.0)
            project.networking_interest_radius = net_data.get("interest_radius", 0.0)
            project.networking_protocol_version = net_data.get("protocol_version", 1)
            project.networking_game_id = net_data.get("game_id", "lupine-game")
            project.networking_enable_lan_discovery = net_data.get("enable_lan_discovery", False)
            project.networking_discovery_port = net_data.get("discovery_port", 7779)
            project.networking_server_name = net_data.get("server_name", "Lupine Server")
            project.networking_enable_prediction = net_data.get("enable_prediction", True)
            project.networking_input_redundancy = net_data.get("input_redundancy", 3)
            project.networking_auto_reconnect = net_data.get("auto_reconnect", False)
            project.networking_reconnect_attempts = net_data.get("reconnect_attempts", 3)
            project.networking_reconnect_delay_seconds = net_data.get("reconnect_delay_seconds", 2.0)
            project.networking_resume_timeout_seconds = net_data.get("resume_timeout_seconds", 0.0)
            project.networking_max_messages_per_second = net_data.get("max_messages_per_second", 0)
            project.networking_max_bytes_per_second = net_data.get("max_bytes_per_second", 0)

        if "splash_screens" in data:
            splash_data = data["splash_screens"]
            project.splash_screens_enabled = splash_data.get("enabled", True)
            project.splash_screens_allow_skip = splash_data.get("allow_skip", True)
            project.splash_screen_entries = splash_data.get("entries", [])

        if "rendering" in data:
            rendering_data = data["rendering"]
            project.default_font = rendering_data.get("default_font", "")
            project.default_theme = rendering_data.get("default_theme", "")
            project.clear_color = rendering_data.get("clear_color", [0.1, 0.1, 0.1, 1.0])

        if "localization" in data and isinstance(data["localization"], dict):
            project.localization.update(data["localization"])

        return project
    
    def get_directory(self) -> str:
        """Get the project directory path"""
        if self.path:
            return str(Path(self.path).parent)
        return ""
    
    def get_icon_full_path(self) -> Optional[str]:
        """Get the full path to the project icon"""
        if self.icon_path:
            relative = self.icon_path.replace("\\", "/")
            if relative.startswith("res://"):
                relative = relative[len("res://"):]
            icon_path = Path(self.get_directory()) / relative
            if icon_path.exists():
                return str(icon_path)
        return None


class ProjectFile:
    """Handles reading and writing .lupine project files"""

    @staticmethod
    def get_engine_default_font_path() -> Optional[str]:
        """Get the path to the engine's default font file"""
        # The default font is located in editor/resources/DefaultFont.ttf
        editor_dir = Path(__file__).parent
        default_font = editor_dir / "resources" / "DefaultFont.ttf"
        if default_font.exists():
            return str(default_font)
        return None

    @staticmethod
    def get_engine_default_theme_path() -> Optional[str]:
        """Get the path to the engine's starter UI theme file."""
        editor_dir = Path(__file__).parent
        default_theme = editor_dir / "resources" / "default.uitheme"
        if default_theme.exists():
            return str(default_theme)
        return None

    @staticmethod
    def get_engine_default_palette_path() -> Optional[str]:
        """Get the path to the engine's starter color palette file."""
        editor_dir = Path(__file__).parent
        default_palette = editor_dir / "resources" / "default.palette"
        if default_palette.exists():
            return str(default_palette)
        return None

    @staticmethod
    def get_engine_default_icon_path() -> Optional[str]:
        """Get the path to the engine's default project icon."""
        editor_dir = Path(__file__).parent
        default_icon = editor_dir / "resources" / "icon.png"
        if default_icon.exists():
            return str(default_icon)
        return None

    @staticmethod
    def create_new_project(name: str, location: str, creator: str = "",
                           icon_source: Optional[str] = None) -> ProjectData:
        """
        Create a new project with the given parameters

        Args:
            name: The project name
            location: The directory where the project folder will be created
            creator: The creator's name (optional)
            icon_source: Path to an image to use as the project icon. When
                omitted (or missing), the engine's default icon is copied in.

        Returns:
            A ProjectData object representing the new project
        """
        # Create project folder
        project_dir = Path(location) / name
        project_dir.mkdir(parents=True, exist_ok=True)
        
        # Create project subdirectories
        (project_dir / "scenes").mkdir(exist_ok=True)
        (project_dir / "assets").mkdir(exist_ok=True)
        (project_dir / "scripts").mkdir(exist_ok=True)
        
        # Create project data
        project = ProjectData()
        project.name = name
        project.path = str(project_dir / f"{name}.lupine")
        project.creator = creator
        project.created_date = datetime.now().isoformat()
        project.last_modified = project.created_date
        
        # Save the project file
        ProjectFile.save_project(project)
        
        # Create a default scene
        default_scene_path = project_dir / "scenes" / "main.scene"
        ProjectFile._create_default_scene(default_scene_path)
        project.main_scene = "scenes/main.scene"

        # Copy default font to project assets
        engine_font = ProjectFile.get_engine_default_font_path()
        if engine_font:
            fonts_dir = project_dir / "assets" / "fonts"
            fonts_dir.mkdir(parents=True, exist_ok=True)
            dest_font = fonts_dir / "DefaultFont.ttf"
            shutil.copy2(engine_font, dest_font)
            project.default_font = "res://assets/fonts/DefaultFont.ttf"

        # Copy the starter UI theme + palette to the project root so the default
        # res://default.uitheme (and its res://default.palette) resolve out of the box.
        engine_theme = ProjectFile.get_engine_default_theme_path()
        if engine_theme:
            shutil.copy2(engine_theme, project_dir / "default.uitheme")
            project.default_theme = "res://default.uitheme"
        engine_palette = ProjectFile.get_engine_default_palette_path()
        if engine_palette:
            shutil.copy2(engine_palette, project_dir / "default.palette")

        # Copy the project icon into the project (per-game icon). Falls back to
        # the engine's default icon when no custom icon was supplied.
        ProjectFile.set_project_icon(project, icon_source)

        ProjectFile.save_project(project)

        return project

    @staticmethod
    def set_project_icon(project: ProjectData, icon_source: Optional[str] = None) -> bool:
        """
        Copy an image into the project and register it as the project icon.

        Args:
            project: The project to assign the icon to (must have a valid path).
            icon_source: Path to the source image. When omitted or missing, the
                engine's default icon is used.

        Returns:
            True when an icon was copied and registered, False otherwise.
        """
        if not project.path:
            return False

        source = icon_source
        if not source or not os.path.exists(source):
            source = ProjectFile.get_engine_default_icon_path()

        if not source or not os.path.exists(source):
            return False

        project_dir = Path(project.get_directory())
        extension = Path(source).suffix.lower() or ".png"
        dest_name = f"icon{extension}"
        dest_path = project_dir / dest_name

        try:
            if os.path.abspath(source) != os.path.abspath(str(dest_path)):
                shutil.copy2(source, dest_path)
            project.icon_path = dest_name
            return True
        except Exception as e:
            print(f"Error copying project icon: {e}")
            return False
    
    @staticmethod
    def _create_default_scene(scene_path: Path):
        """Create a default empty scene file"""
        default_scene = {
            "scene": {
                "name": "Main Scene",
                "nodes": []
            }
        }
        with open(scene_path, 'w') as f:
            json.dump(default_scene, f, indent=2)
    
    @staticmethod
    def load_project(project_path: str) -> Optional[ProjectData]:
        """
        Load a project from a .lupine file
        
        Args:
            project_path: Path to the .lupine file
        
        Returns:
            ProjectData object if successful, None otherwise
        """
        try:
            path = Path(project_path)
            if not path.exists():
                print(f"Project file not found: {project_path}")
                return None
            
            if path.suffix != ".lupine":
                print(f"Invalid project file extension: {path.suffix}")
                return None
            
            with open(path, 'r') as f:
                data = json.load(f)
            
            project = ProjectData.from_dict(data, str(path))
            
            # Update last modified time
            project.last_modified = datetime.now().isoformat()
            ProjectFile.save_project(project)
            
            return project
            
        except json.JSONDecodeError as e:
            print(f"Error parsing project file: {e}")
            return None
        except Exception as e:
            print(f"Error loading project: {e}")
            return None
    
    @staticmethod
    def save_project(project: ProjectData) -> bool:
        """
        Save a project to a .lupine file
        
        Args:
            project: The ProjectData to save
        
        Returns:
            True if successful, False otherwise
        """
        try:
            if not project.path:
                print("Project path not set")
                return False
            
            project.last_modified = datetime.now().isoformat()
            
            with open(project.path, 'w') as f:
                json.dump(project.to_dict(), f, indent=2)
            
            return True
            
        except Exception as e:
            print(f"Error saving project: {e}")
            return False
    
    @staticmethod
    def validate_project_path(path: str) -> bool:
        """
        Check if a path points to a valid .lupine file
        
        Args:
            path: Path to check
        
        Returns:
            True if valid, False otherwise
        """
        path_obj = Path(path)
        return path_obj.exists() and path_obj.suffix == ".lupine"
    
    @staticmethod
    def get_recent_projects_file() -> str:
        """Get the path to the recent projects JSON file"""
        # Store in user's home directory
        home = Path.home()
        lupine_dir = home / ".lupine"
        lupine_dir.mkdir(exist_ok=True)
        return str(lupine_dir / "recent_projects.json")
    
    @staticmethod
    def load_recent_projects() -> list:
        """Load the list of recent projects"""
        recent_file = ProjectFile.get_recent_projects_file()
        
        if not os.path.exists(recent_file):
            return []
        
        try:
            with open(recent_file, 'r') as f:
                data = json.load(f)
                return data.get("recent_projects", [])
        except Exception as e:
            print(f"Error loading recent projects: {e}")
            return []
    
    @staticmethod
    def save_recent_projects(projects: list):
        """Save the list of recent projects"""
        recent_file = ProjectFile.get_recent_projects_file()
        
        try:
            with open(recent_file, 'w') as f:
                json.dump({"recent_projects": projects}, f, indent=2)
        except Exception as e:
            print(f"Error saving recent projects: {e}")
    
    @staticmethod
    def add_to_recent_projects(project_path: str, project_name: str):
        """Add a project to the recent projects list"""
        recent = ProjectFile.load_recent_projects()
        
        # Remove if already exists
        recent = [p for p in recent if p.get("path") != project_path]
        
        # Add to front
        recent.insert(0, {
            "path": project_path,
            "name": project_name,
            "last_opened": datetime.now().isoformat()
        })
        
        # Keep only last 10
        recent = recent[:10]
        
        ProjectFile.save_recent_projects(recent)
