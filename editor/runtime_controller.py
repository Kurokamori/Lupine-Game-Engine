"""
Lupine Runtime Controller

High-level Python wrapper for controlling the Lupine game runtime from the editor.
Provides convenient methods for playing games, scenes, and managing playback.
"""

import threading
import time
from typing import Optional, Dict, Any
from pathlib import Path
from PyQt6.QtCore import QTimer

try:
    import lupine_runtime as lr
except ImportError:
    print("Warning: lupine_runtime module not found. Make sure it's built and in the path.")
    lr = None


class RuntimeController:
    """
    High-level controller for the Lupine runtime.

    Manages the lifecycle of the runtime application and provides
    convenient methods for playing games and scenes from the editor.
    """

    def __init__(self):
        """Initialize the runtime controller."""
        self._app: Optional[lr.RuntimeApp] = None
        self._last_config: Optional[lr.RuntimeConfig] = None
        self._is_async = False
        self._event_timer: Optional[QTimer] = None
        self._event_process_count = 0  # Debug counter

    def play_game(self,
                  project_path: str,
                  window_width: int = 1280,
                  window_height: int = 720,
                  debugging: bool = False,
                  vsync: bool = True,
                  resizable: bool = True,
                  blocking: bool = False) -> bool:
        """
        Play a game from a project file.

        Loads the project and runs its main scene.

        Args:
            project_path: Path to the .lupine project file
            window_width: Width of the game window
            window_height: Height of the game window
            debugging: Enable debug logging
            vsync: Enable vertical sync
            resizable: Make window resizable
            blocking: If True, blocks until game exits. If False, runs asynchronously.

        Returns:
            True if successfully started, False otherwise
        """
        if not lr:
            print("Error: lupine_runtime module not available")
            return False

        # Stop and cleanup any existing runtime
        if self._app:
            if self._app.is_running():
                self._app.stop()
            # Explicitly call shutdown() to ensure proper cleanup before creating new instance
            # This prevents race conditions with the SceneManager singleton
            self._app.shutdown()
            self._app = None
            # Give SDL/OpenGL/Logger time to fully cleanup before creating new instance
            time.sleep(0.1)

        # Create config
        config = lr.RuntimeConfig()
        config.title = f"Lupine Runtime - {Path(project_path).stem}"
        config.window_width = window_width
        config.window_height = window_height
        config.debugging = debugging
        config.vsync = vsync
        config.resizable = resizable
        config.project_path = str(project_path)
        config.scene_path = ""  # Empty = use main scene from project

        # Store config for relaunching
        self._last_config = config

        # Create and initialize runtime
        self._app = lr.RuntimeApp()
        if not self._app.initialize(config):
            print(f"Error: Failed to initialize runtime with project: {project_path}")
            self._app = None
            return False

        print(f"Playing game: {project_path}")

        # Run
        if blocking:
            self._is_async = False
            self._app.run()
        else:
            self._is_async = True
            self._app.run_async()
            # Start Qt timer to process SDL events on main thread
            self._start_event_processing()

        return True

    def play_scene(self,
                   project_path: str,
                   scene_path: str,
                   window_width: int = 1280,
                   window_height: int = 720,
                   debugging: bool = False,
                   vsync: bool = True,
                   resizable: bool = True,
                   blocking: bool = False) -> bool:
        """
        Play a specific scene from a project.

        Loads the project but runs the specified scene instead of the main scene.

        Args:
            project_path: Path to the .lupine project file
            scene_path: Path to the .scene file to play
            window_width: Width of the game window
            window_height: Height of the game window
            debugging: Enable debug logging
            vsync: Enable vertical sync
            resizable: Make window resizable
            blocking: If True, blocks until game exits. If False, runs asynchronously.

        Returns:
            True if successfully started, False otherwise
        """
        if not lr:
            print("Error: lupine_runtime module not available")
            return False

        # Stop and cleanup any existing runtime
        if self._app:
            if self._app.is_running():
                self._app.stop()
            # Explicitly call shutdown() to ensure proper cleanup before creating new instance
            # This prevents race conditions with the SceneManager singleton
            self._app.shutdown()
            self._app = None
            # Give SDL/OpenGL/Logger time to fully cleanup before creating new instance
            time.sleep(0.1)

        # Create config
        config = lr.RuntimeConfig()
        config.title = f"Lupine Runtime - {Path(scene_path).stem}"
        config.window_width = window_width
        config.window_height = window_height
        config.debugging = debugging
        config.vsync = vsync
        config.resizable = resizable
        config.project_path = str(project_path)
        config.scene_path = str(scene_path)

        # Store config for relaunching
        self._last_config = config

        # Create and initialize runtime
        self._app = lr.RuntimeApp()
        if not self._app.initialize(config):
            print(f"Error: Failed to initialize runtime with scene: {scene_path}")
            self._app = None
            return False

        print(f"Playing scene: {scene_path} (project: {project_path})")

        # Run
        if blocking:
            self._is_async = False
            self._app.run()
        else:
            self._is_async = True
            self._app.run_async()
            # Start Qt timer to process SDL events on main thread
            self._start_event_processing()

        return True

    def _start_event_processing(self) -> None:
        """Start Qt timer to process SDL events on the main thread."""
        if not self._event_timer and self._app:
            self._event_timer = QTimer()
            self._event_timer.timeout.connect(self._process_runtime_events)
            # Process events as fast as possible (0ms = process whenever Qt event loop is idle)
            # This is CRITICAL for Windows - SDL window messages must be processed frequently
            # from the thread that created the window, or the window will hang when clicked/moved
            self._event_timer.start(0)
            print("Started SDL event processing timer (0ms interval = as fast as possible)")

    def _stop_event_processing(self) -> None:
        """Stop the event processing timer."""
        if self._event_timer:
            self._event_timer.stop()
            self._event_timer = None
            self._event_process_count = 0  # Reset counter
            print("Stopped SDL event processing timer")

    def _process_runtime_events(self) -> None:
        """Process SDL events for the runtime (called by Qt timer on main thread)."""
        if not self._app:
            self._stop_event_processing()
            return

        try:
            # CRITICAL: Process SDL events on the main thread
            # SDL requires event processing to happen on the thread that created the window
            # When running async, the runtime thread handles update/physics/render,
            # but the main thread (Qt main thread) must handle SDL events
            if not self._app.process_events():
                print("Runtime window closed - initiating cleanup...")
                self._stop_event_processing()
                # Cleanup the runtime object
                self._app.stop()
                time.sleep(0.1)  # Brief delay for thread to finish
                self._app = None
                print("Runtime cleanup complete")
                return

            # Check if runtime has stopped (e.g., user closed window)
            if not self._app.is_running():
                print("Runtime stopped - initiating cleanup...")
                self._stop_event_processing()
                # Cleanup the runtime object
                time.sleep(0.1)  # Brief delay for thread to finish
                self._app = None
                print("Runtime cleanup complete")
                return
        except Exception as e:
            print(f"Error processing runtime events: {e}")
            import traceback
            traceback.print_exc()
            self._stop_event_processing()

    def stop(self) -> None:
        """
        Stop the runtime.

        Gracefully stops the game loop and cleans up resources.
        """
        self._stop_event_processing()  # Stop event processing first

        if self._app:
            print("Stopping runtime...")
            self._app.stop()
            # Give a moment for the thread to fully exit before cleanup
            time.sleep(0.1)
            # Explicitly call shutdown() to ensure proper cleanup
            # This prevents race conditions with the SceneManager singleton
            self._app.shutdown()
            self._app = None
        else:
            print("No runtime to stop")

    def pause(self) -> None:
        """
        Pause the runtime.

        Pauses game updates and physics, but continues rendering.
        """
        if self._app:
            if not self._app.is_paused():
                print("Pausing runtime...")
                self._app.pause()
            else:
                print("Runtime already paused")
        else:
            print("No runtime to pause")

    def resume(self) -> None:
        """
        Resume the runtime from paused state.

        Resumes game updates and physics.
        """
        if self._app:
            if self._app.is_paused():
                print("Resuming runtime...")
                self._app.resume()
            else:
                print("Runtime not paused")
        else:
            print("No runtime to resume")

    def relaunch(self, blocking: bool = False) -> bool:
        """
        Relaunch the runtime with the same configuration as last time.

        Useful for quickly restarting the game after making changes.

        Args:
            blocking: If True, blocks until game exits. If False, runs asynchronously.

        Returns:
            True if successfully relaunched, False otherwise
        """
        if not self._last_config:
            print("Error: No previous configuration to relaunch")
            return False

        print("Relaunching runtime with previous configuration...")

        # Stop and cleanup any existing runtime
        if self._app:
            if self._app.is_running():
                self._app.stop()
            # Explicitly call shutdown() to ensure proper cleanup before creating new instance
            # This prevents race conditions with the SceneManager singleton
            self._app.shutdown()
            self._app = None
            # Give SDL/OpenGL/Logger time to fully cleanup before creating new instance
            time.sleep(0.1)

        # Create and initialize runtime with saved config
        self._app = lr.RuntimeApp()
        if not self._app.initialize(self._last_config):
            print("Error: Failed to initialize runtime for relaunch")
            self._app = None
            return False

        # Run
        if blocking:
            self._is_async = False
            self._app.run()
        else:
            self._is_async = True
            self._app.run_async()
            # Start Qt timer to process SDL events on main thread
            self._start_event_processing()

        return True

    def reload_scene(self) -> bool:
        """
        Reload the current scene.

        Useful for seeing changes without fully restarting the runtime.

        Returns:
            True if successfully reloaded, False otherwise
        """
        if self._app:
            print("Reloading scene...")
            self._app.reload_scene()
            return True
        else:
            print("No runtime to reload scene")
            return False

    def load_scene(self, scene_path: str) -> bool:
        """
        Load a different scene in the running runtime.

        Args:
            scene_path: Path to the .scene file to load

        Returns:
            True if successfully loaded, False otherwise
        """
        if self._app:
            print(f"Loading scene: {scene_path}")
            return self._app.load_scene(str(scene_path))
        else:
            print("No runtime to load scene into")
            return False

    def is_running(self) -> bool:
        """
        Check if the runtime is currently running.

        Returns:
            True if running, False otherwise
        """
        if self._app:
            return self._app.is_running()
        return False

    def is_paused(self) -> bool:
        """
        Check if the runtime is currently paused.

        Returns:
            True if paused, False otherwise
        """
        if self._app:
            return self._app.is_paused()
        return False

    def get_status(self) -> Dict[str, Any]:
        """
        Get the current status of the runtime.

        Returns:
            Dictionary containing status information
        """
        if not self._app:
            return {
                "active": False,
                "running": False,
                "paused": False,
                "async": False,
                "config": None
            }

        return {
            "active": True,
            "running": self._app.is_running(),
            "paused": self._app.is_paused(),
            "async": self._is_async,
            "config": {
                "title": self._last_config.title if self._last_config else None,
                "project_path": self._last_config.project_path if self._last_config else None,
                "scene_path": self._last_config.scene_path if self._last_config else None,
            }
        }

    def __del__(self):
        """Cleanup on deletion."""
        if self._app and self._app.is_running():
            self.stop()


# Global singleton instance for convenience
_controller = None


def get_controller() -> RuntimeController:
    """
    Get the global RuntimeController singleton.

    Returns:
        The global RuntimeController instance
    """
    global _controller
    if _controller is None:
        _controller = RuntimeController()
    return _controller


# Convenience functions that use the global controller
def play_game(project_path: str, **kwargs) -> bool:
    """Play a game using the global controller."""
    return get_controller().play_game(project_path, **kwargs)


def play_scene(project_path: str, scene_path: str, **kwargs) -> bool:
    """Play a scene using the global controller."""
    return get_controller().play_scene(project_path, scene_path, **kwargs)


def stop() -> None:
    """Stop the runtime using the global controller."""
    get_controller().stop()


def pause() -> None:
    """Pause the runtime using the global controller."""
    get_controller().pause()


def resume() -> None:
    """Resume the runtime using the global controller."""
    get_controller().resume()


def relaunch(**kwargs) -> bool:
    """Relaunch the runtime using the global controller."""
    return get_controller().relaunch(**kwargs)


def reload_scene() -> bool:
    """Reload the current scene using the global controller."""
    return get_controller().reload_scene()


def load_scene(scene_path: str) -> bool:
    """Load a different scene using the global controller."""
    return get_controller().load_scene(scene_path)


def is_running() -> bool:
    """Check if runtime is running using the global controller."""
    return get_controller().is_running()


def is_paused() -> bool:
    """Check if runtime is paused using the global controller."""
    return get_controller().is_paused()


def get_status() -> Dict[str, Any]:
    """Get runtime status using the global controller."""
    return get_controller().get_status()


# Example usage
if __name__ == "__main__":
    # Example: Play a game
    # play_game("path/to/project.lupine", debugging=True, blocking=False)

    # Example: Play a specific scene
    # play_scene("path/to/project.lupine", "path/to/scene.scene", debugging=True)

    # Example: Control playback
    # pause()
    # time.sleep(2)
    # resume()
    # time.sleep(5)
    # stop()

    print("Runtime controller module loaded successfully!")
    print("Import this module in your editor to control the game runtime.")
