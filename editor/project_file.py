"""
Lupine Engine Project File Handler
Manages .lupine project files and project data structures
"""

import json
import os
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

        # Graphics settings
        self.scale_mode: str = "letterbox"  # letterbox, stretch, crop, ignore
        self.texture_filtering: str = "bilinear"  # cubic, bilinear, nearest

        # Editor settings
        self.save_on_play: bool = True
        self.max_undo_steps: int = 100

        # Input mapping
        self.input_map: Dict = {"actions": [], "axes": []}
    
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
            },
            "graphics": {
                "scale_mode": self.scale_mode,
                "texture_filtering": self.texture_filtering,
            },
            "editor": {
                "save_on_play": self.save_on_play,
                "max_undo_steps": self.max_undo_steps,
            },
            "input_map": self.input_map
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

        if "graphics" in data:
            graphics_data = data["graphics"]
            project.scale_mode = graphics_data.get("scale_mode", "letterbox")
            project.texture_filtering = graphics_data.get("texture_filtering", "bilinear")

        if "editor" in data:
            editor_data = data["editor"]
            project.save_on_play = editor_data.get("save_on_play", True)
            project.max_undo_steps = editor_data.get("max_undo_steps", 100)

        if "input_map" in data:
            project.input_map = data["input_map"]

        return project
    
    def get_directory(self) -> str:
        """Get the project directory path"""
        if self.path:
            return str(Path(self.path).parent)
        return ""
    
    def get_icon_full_path(self) -> Optional[str]:
        """Get the full path to the project icon"""
        if self.icon_path:
            icon_path = Path(self.get_directory()) / self.icon_path
            if icon_path.exists():
                return str(icon_path)
        return None


class ProjectFile:
    """Handles reading and writing .lupine project files"""
    
    @staticmethod
    def create_new_project(name: str, location: str, creator: str = "") -> ProjectData:
        """
        Create a new project with the given parameters
        
        Args:
            name: The project name
            location: The directory where the project folder will be created
            creator: The creator's name (optional)
        
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
        ProjectFile.save_project(project)
        
        return project
    
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
