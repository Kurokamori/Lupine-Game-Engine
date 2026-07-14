"""
Lupine Runtime Controller

High-level Python wrapper for controlling the Lupine game runtime from the
editor. Every "Play" launch now runs the game in its own OS process with its own
window, its own engine singletons (VFS / InputManager / script hosts / graphics
device) and its own main thread - whether one instance or many. The editor
drives those processes (pause / resume / step / stop / reload-scene /
load-scene / reload-scripts) over an IPC control channel (see runtime_ipc), and
mirrors each instance's log output into the editor console through that same
channel.

This replaces the previous "single instance in-process, many out-of-process"
split: in-process play forced the game to share the editor's process singletons
(SDL, OpenGL, VFS, script hosts), which made it impossible to give the game its
own clean resources and its own main thread. Going always-out-of-process fixes
that and makes single- and multi-instance play behave identically.
"""

import os
import sys
import json
import shlex
import subprocess
import threading
import time
from collections import OrderedDict
from typing import Optional, Dict, Any, List, Tuple
from pathlib import Path
from PyQt6.QtCore import QTimer

import runtime_ipc

try:
    import lupine_runtime as lr
except ImportError:
    print("Warning: lupine_runtime module not found. Make sure it's built and in the path.")
    lr = None

try:
    import lupine_engine as le
except ImportError:
    le = None


# Profiler sources currently available for the editor's ProfilerPanel to read.
# Each entry is {"index": int, "label": str, "path": Optional[str]}. Every play
# instance now runs out-of-process, so each source carries a path to the JSON
# dump that instance streams its profiler history to; the panel polls those
# files. (There is no longer an in-process "path is None, read live" source.)
# Published here (module-level rather than on the controller) because the editor
# builds its own RuntimeController instance and the panel has no direct handle.
_profiler_sources: List[Dict[str, Any]] = []


def get_profiler_sources() -> List[Dict[str, Any]]:
    """Return the profiler sources for the currently active play session."""
    return list(_profiler_sources)


def _set_profiler_sources(sources: List[Dict[str, Any]]) -> None:
    global _profiler_sources
    _profiler_sources = sources


class RuntimeController:
    """
    High-level controller for the Lupine runtime.

    Manages the lifecycle of one or more out-of-process game instances and
    provides convenient methods for playing games and scenes from the editor and
    controlling them (pause/resume/step/stop/reload) over the IPC channel.
    """

    def __init__(self):
        """Initialize the runtime controller."""
        self._log_callback = None  # callback(message: str, level: str)

        # IPC server for the active play session; one per session. Children
        # connect back to it to receive commands and stream logs/status.
        self._server: Optional[runtime_ipc.IpcServer] = None

        # Out-of-process game instances. Each record:
        # {"proc": Popen, "index": int, "label": str, "dump_path": str,
        #  "paused": bool, "closed": bool, "pid": Optional[int]}.
        self._instances: List[Dict[str, Any]] = []

        # Spawn parameters of the last session, for relaunch(). Kept truthy so
        # the editor's "Relaunch" button can enable on `_last_config is not None`
        # (the attribute name is retained for editor compatibility).
        self._last_config: Optional[Dict[str, Any]] = None

        # Aggregate paused intent, kept in sync with status events. Drives the
        # editor's Pause/Resume toggle without a round-trip per poll.
        self._paused = False

        # Single Qt timer that drains IPC events (logs/status), prunes exited
        # processes, and tears the session down when the last instance exits.
        self._session_timer: Optional[QTimer] = None

        # Live-edit coalescing. Editor edits (node/component properties, node
        # transforms, asset reloads) made while the game is running are queued
        # here keyed by target so a rapid drag collapses to the latest value,
        # then flushed to every instance over IPC on a short timer. This keeps
        # high-frequency edits from flooding the control channel.
        self._pending_edits: "OrderedDict[Tuple, Dict[str, Any]]" = OrderedDict()
        self._pending_edits_lock = threading.Lock()
        self._edit_flush_timer: Optional[QTimer] = None

    def set_log_callback(self, callback) -> None:
        """Set a callback that receives runtime log lines as (message, level)."""
        self._log_callback = callback

    # ------------------------------------------------------------------
    # Backend helpers
    # ------------------------------------------------------------------

    def _backend_name(self, backend) -> str:
        """Resolve a graphics backend to its canonical name string for the runner."""
        if backend is None:
            return ""
        name = getattr(backend, "name", None)
        if name:
            return str(name)
        return str(backend)

    # ------------------------------------------------------------------
    # Per-instance paths and environment
    # ------------------------------------------------------------------

    def _base_user_data_path(self) -> str:
        """Resolve the platform default user:// directory (matches the engine)."""
        if lr is not None and hasattr(lr, "get_default_user_data_path"):
            try:
                base = lr.get_default_user_data_path()
                if base:
                    return base
            except Exception:
                pass
        # Fallback mirrors VirtualFileSystem::GetDefaultUserDataPath ("Lupine").
        appdata = os.environ.get("APPDATA")
        root = appdata if appdata else str(Path.home())
        return os.path.join(root, "Lupine")

    def _safe_stem(self, project_path: str) -> str:
        stem = Path(project_path).stem or "project"
        safe = "".join(c for c in stem if c.isalnum() or c in ("-", "_", " ")).strip()
        return safe or "project"

    def _compute_user_path(self, project_path: str, index: int, unique: bool) -> str:
        """
        Per-instance user:// directory under the base user data path.

        Layout: <base>/instances/<project>/instance_<index>. Returns "" when
        unique directories are disabled (instances then share the default).
        """
        if not unique:
            return ""
        base = self._base_user_data_path()
        return os.path.join(base, "instances", self._safe_stem(project_path),
                            f"instance_{index}")

    def _profiler_dump_path(self, project_path: str, index: int) -> str:
        """
        Per-instance file an out-of-process instance streams its profiler history
        to, under <base>/profiler/<project>/instance_<index>.json. The editor's
        ProfilerPanel reads this file for the instance's tab.
        """
        base = self._base_user_data_path()
        folder = os.path.join(base, "profiler", self._safe_stem(project_path))
        try:
            os.makedirs(folder, exist_ok=True)
        except OSError:
            pass
        return os.path.join(folder, f"instance_{index}.json")

    def _build_instance_args(self, runtime_args: str, index: int) -> List[str]:
        """
        Split the user's runtime-args string into tokens, substituting the
        {instance} placeholder with the zero-based instance index.
        """
        if not runtime_args:
            return []
        try:
            tokens = shlex.split(runtime_args, posix=(os.name != "nt"))
        except ValueError:
            tokens = runtime_args.split()
        return [token.replace("{instance}", str(index)) for token in tokens]

    def _runner_environment(self) -> Dict[str, str]:
        """Environment for spawned instances: replicate the editor's import path."""
        env = dict(os.environ)
        paths = [p for p in sys.path if p]
        for module in (lr, le):
            mod_file = getattr(module, "__file__", None) if module else None
            if mod_file:
                mod_dir = os.path.dirname(mod_file)
                if mod_dir and mod_dir not in paths:
                    paths.append(mod_dir)
        env["LUPINE_RUNNER_SYSPATH"] = os.pathsep.join(paths)
        return env

    # ------------------------------------------------------------------
    # Process spawning
    # ------------------------------------------------------------------

    def _spawn_instance(self, project_path: str, scene_path: str, backend,
                        index: int, count: int, unique_user_dir: bool,
                        runtime_args: str, window_width: int, window_height: int,
                        debugging: bool) -> Optional[Dict[str, Any]]:
        """Launch one game instance as a separate OS process wired to the IPC
        server. Returns a record on success, or None on failure."""
        runner = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                              "runtime_instance_runner.py")
        if not os.path.exists(runner):
            print(f"Error: runtime instance runner not found at {runner}")
            return None

        if count > 1:
            title = f"Lupine Runtime - {Path(project_path).stem} [{index + 1}/{count}]"
        else:
            title = f"Lupine Runtime - {Path(project_path).stem}"

        cmd = [sys.executable, runner,
               "--project", str(project_path),
               "--width", str(window_width),
               "--height", str(window_height),
               "--title", title,
               "--instance", str(index)]

        if scene_path:
            cmd += ["--scene", str(scene_path)]

        backend_name = self._backend_name(backend)
        if backend_name:
            cmd += ["--backend", backend_name]

        if debugging:
            cmd += ["--debug"]

        # Wire the instance to the editor's IPC control channel.
        if self._server is not None:
            cmd += ["--ipc-host", self._server.host,
                    "--ipc-port", str(self._server.port),
                    "--ipc-token", self._server.token]

        # Hand the instance a file to stream its profiler history to. Clear any
        # stale dump from a previous session so the panel does not show old data
        # before the instance writes its first snapshot.
        dump_path = self._profiler_dump_path(project_path, index)
        try:
            if os.path.exists(dump_path):
                os.remove(dump_path)
        except OSError:
            pass
        cmd += ["--profiler-dump", dump_path]

        user_path = self._compute_user_path(project_path, index, unique_user_dir)
        if user_path:
            try:
                os.makedirs(user_path, exist_ok=True)
            except OSError as exc:
                print(f"Warning: could not create user dir {user_path}: {exc}")
            cmd += ["--user-path", user_path]

        game_args = self._build_instance_args(runtime_args, index)
        if game_args:
            cmd += ["--"] + game_args

        # Run each instance from its own user directory when isolated, so the
        # runtime's lupine_runtime.log (written to CWD) does not clobber the
        # other instances' logs. res:// / input maps use absolute paths, so CWD
        # does not affect resource resolution.
        cwd = user_path if (user_path and os.path.isdir(user_path)) else None

        try:
            proc = subprocess.Popen(cmd, env=self._runner_environment(), cwd=cwd)
            print(f"Launched instance {index + 1}/{count} (pid {proc.pid})")
            label = f"Instance {index + 1}" if count > 1 else "Game"
            return {
                "proc": proc,
                "index": index,
                "label": label,
                "dump_path": dump_path,
                "paused": False,
                "closed": False,
                "pid": proc.pid,
            }
        except Exception as exc:
            print(f"Error launching instance {index + 1}/{count}: {exc}")
            return None

    def _launch(self, project_path: str, scene_path: str, backend, count: int,
                unique_user_dir: bool, runtime_args: str, window_width: int,
                window_height: int, debugging: bool, blocking: bool) -> bool:
        """Tear down any current session and spawn `count` isolated game
        processes wired to a fresh IPC server. Returns True if any started."""
        self._teardown_session()

        self._server = runtime_ipc.IpcServer()
        self._paused = False

        for index in range(count):
            record = self._spawn_instance(
                project_path, scene_path, backend, index, count,
                unique_user_dir, runtime_args, window_width, window_height, debugging)
            if record is not None:
                self._instances.append(record)

        if not self._instances:
            self._teardown_session()
            return False

        self._publish_profiler_sources()
        self._start_session_watch()

        if blocking:
            self._wait_for_instances()

        return True

    def _wait_for_instances(self) -> None:
        """Block until every spawned instance exits (CLI / blocking use)."""
        for rec in list(self._instances):
            try:
                rec["proc"].wait()
            except Exception:
                pass
        self._teardown_session()

    # ------------------------------------------------------------------
    # Session bookkeeping (timer, profiler sources, teardown)
    # ------------------------------------------------------------------

    def _publish_profiler_sources(self) -> None:
        """Expose the live instances' profiler dump files to the ProfilerPanel."""
        _set_profiler_sources([
            {"index": rec["index"], "label": rec["label"], "path": rec["dump_path"]}
            for rec in self._instances
        ])

    def _start_session_watch(self) -> None:
        """Start the per-session timer that drains IPC events + prunes exits."""
        if self._session_timer is None:
            self._session_timer = QTimer()
            self._session_timer.timeout.connect(self._on_session_tick)
            self._session_timer.start(30)

    def _stop_session_watch(self) -> None:
        if self._session_timer is not None:
            self._session_timer.stop()
            self._session_timer = None

    def _on_session_tick(self) -> None:
        """Drain IPC events, mirror logs, then prune exited processes."""
        self._drain_ipc_events()
        self._prune_processes()

    def _multi(self) -> bool:
        return len(self._instances) > 1

    def _drain_ipc_events(self) -> None:
        """Pull buffered child events and mirror logs/status into the editor."""
        if self._server is None:
            return
        for index, event in self._server.drain_events():
            kind = event.get("event", "")
            rec = self._record_for(index)
            if kind == runtime_ipc.EVENT_LOG:
                self._mirror_log(rec, event.get("message", ""),
                                 event.get("level", "Info"))
            elif kind == runtime_ipc.EVENT_STATUS:
                if rec is not None:
                    rec["paused"] = bool(event.get("paused", False))
                self._reconcile_paused()
            elif kind == runtime_ipc.EVENT_HELLO:
                if rec is not None:
                    rec["pid"] = event.get("pid", rec.get("pid"))
            elif kind == runtime_ipc.EVENT_CLOSED:
                if rec is not None:
                    rec["closed"] = True

    def _record_for(self, index: int) -> Optional[Dict[str, Any]]:
        for rec in self._instances:
            if rec["index"] == index:
                return rec
        return None

    def _mirror_log(self, rec: Optional[Dict[str, Any]], message: str,
                    level: str) -> None:
        if not self._log_callback or not message:
            return
        # Tag lines with the instance label only when several are running, so a
        # single-instance session reads exactly like the old in-process mirror.
        if rec is not None and self._multi():
            message = f"[{rec['label']}] {message}"
        self._log_callback(message, level)

    def _reconcile_paused(self) -> None:
        """Derive the aggregate paused flag from connected instances' status."""
        if not self._instances:
            return
        self._paused = all(rec.get("paused", False) for rec in self._instances)

    def _prune_processes(self) -> None:
        """Drop instances whose process exited; tear the session down at zero."""
        before = len(self._instances)
        self._instances = [rec for rec in self._instances
                           if rec["proc"].poll() is None]
        if len(self._instances) != before:
            self._publish_profiler_sources()
        if not self._instances:
            self._teardown_session()

    def _broadcast(self, command: str, **fields: Any) -> int:
        """Send a command to all connected instances; 0 if no live session."""
        if self._server is None:
            return 0
        return self._server.broadcast(command, **fields)

    def _terminate_processes(self) -> None:
        """Ask instances to stop, then terminate/kill any stragglers."""
        # Prefer a graceful IPC stop so each instance shuts its runtime down
        # cleanly (flush logs, release GPU resources) before we escalate.
        self._broadcast(runtime_ipc.CMD_STOP)
        deadline = time.time() + 2.0
        for rec in self._instances:
            proc = rec["proc"]
            remaining = max(0.0, deadline - time.time())
            try:
                proc.wait(timeout=remaining)
            except Exception:
                break  # at least one is slow; fall through to terminate
        for rec in self._instances:
            proc = rec["proc"]
            if proc.poll() is None:
                try:
                    proc.terminate()
                except Exception:
                    pass
        # Final hard-kill pass for anything still alive.
        deadline = time.time() + 1.5
        for rec in self._instances:
            proc = rec["proc"]
            remaining = max(0.0, deadline - time.time())
            try:
                proc.wait(timeout=remaining)
            except Exception:
                try:
                    proc.kill()
                except Exception:
                    pass
        self._instances = []

    def _teardown_session(self) -> None:
        """Stop the timer, terminate instances, and close the IPC server."""
        self._stop_session_watch()
        self._stop_edit_flush_timer()
        with self._pending_edits_lock:
            self._pending_edits.clear()
        self._terminate_processes()
        if self._server is not None:
            self._server.close()
            self._server = None
        self._paused = False
        _set_profiler_sources([])

    # ------------------------------------------------------------------
    # Public play API
    # ------------------------------------------------------------------

    def play_game(self,
                  project_path: str,
                  window_width: int = 1280,
                  window_height: int = 720,
                  debugging: bool = False,
                  vsync: bool = True,
                  resizable: bool = True,
                  blocking: bool = False,
                  backend = None,
                  instance_count: int = 1,
                  runtime_args: str = "",
                  unique_user_dir: bool = True) -> bool:
        """
        Play a game from a project file, running its main scene.

        Always launches out-of-process: one isolated runtime process when
        instance_count is 1, or that many isolated processes for local
        multiplayer/networking testing. Each process has its own window, engine
        singletons and main thread, and is controlled over the IPC channel.

        Args:
            project_path: Path to the .lupine project file.
            window_width / window_height: Game window size.
            debugging: Enable debug logging.
            vsync / resizable: Reserved for parity with the editor call site;
                window pacing is handled by the runtime itself.
            blocking: If True, block until all instances exit (CLI use).
            backend: Graphics backend (lupine_runtime/lupine_engine.GraphicsBackend).
            instance_count: Number of isolated game processes to launch.
            runtime_args: Extra args forwarded to each instance
                (readable via get_cmdline_args()); {instance} expands to the index.
            unique_user_dir: Give each instance its own user:// directory.

        Returns:
            True if at least one instance started, False otherwise.
        """
        return self._play(str(project_path), "", window_width, window_height,
                          debugging, blocking, backend, instance_count,
                          runtime_args, unique_user_dir)

    def play_scene(self,
                   project_path: str,
                   scene_path: str,
                   window_width: int = 1280,
                   window_height: int = 720,
                   debugging: bool = False,
                   vsync: bool = True,
                   resizable: bool = True,
                   blocking: bool = False,
                   backend = None,
                   instance_count: int = 1,
                   runtime_args: str = "",
                   unique_user_dir: bool = True) -> bool:
        """
        Play a specific scene from a project (otherwise identical to play_game).

        Returns:
            True if at least one instance started, False otherwise.
        """
        return self._play(str(project_path), str(scene_path), window_width,
                          window_height, debugging, blocking, backend,
                          instance_count, runtime_args, unique_user_dir)

    def _play(self, project_path: str, scene_path: str, window_width: int,
              window_height: int, debugging: bool, blocking: bool, backend,
              instance_count: int, runtime_args: str,
              unique_user_dir: bool) -> bool:
        """Shared launch path for play_game/play_scene."""
        if not lr:
            print("Error: lupine_runtime module not available")
            return False

        count = max(1, int(instance_count) if instance_count else 1)

        # Remember everything needed to relaunch this exact session.
        self._last_config = {
            "project_path": project_path,
            "scene_path": scene_path,
            "window_width": window_width,
            "window_height": window_height,
            "debugging": debugging,
            "backend": backend,
            "instance_count": count,
            "runtime_args": runtime_args,
            "unique_user_dir": unique_user_dir,
            "title": Path(scene_path).stem if scene_path else Path(project_path).stem,
        }

        started = self._launch(project_path, scene_path, backend, count,
                               unique_user_dir, runtime_args, window_width,
                               window_height, debugging, blocking)
        if started:
            target = scene_path or project_path
            print(f"Playing: {target} ({count} instance(s))")
        return started

    # ------------------------------------------------------------------
    # Playback control (routed to instances over IPC)
    # ------------------------------------------------------------------

    def stop(self) -> None:
        """Stop the runtime: gracefully signal instances, then clean up."""
        if self._instances:
            print("Stopping runtime...")
        self._teardown_session()

    def pause(self) -> None:
        """Pause all instances (updates/physics halt; rendering continues)."""
        if self._broadcast(runtime_ipc.CMD_PAUSE):
            self._paused = True
            print("Pausing runtime...")
        else:
            print("No runtime to pause")

    def resume(self) -> None:
        """Resume all paused instances."""
        if self._broadcast(runtime_ipc.CMD_RESUME):
            self._paused = False
            print("Resuming runtime...")
        else:
            print("No runtime to resume")

    def step(self) -> None:
        """Advance one frame on all instances while keeping them paused."""
        if self._broadcast(runtime_ipc.CMD_STEP):
            self._paused = True
            print("Stepping runtime one frame...")
        else:
            print("No runtime to step")

    def relaunch(self, blocking: bool = False) -> bool:
        """
        Relaunch with the same configuration as the last play session.

        Returns:
            True if successfully relaunched, False if there is no prior session.
        """
        if not self._last_config:
            print("Error: No previous configuration to relaunch")
            return False

        cfg = dict(self._last_config)
        print("Relaunching runtime with previous configuration...")
        return self._launch(
            cfg["project_path"], cfg["scene_path"], cfg["backend"],
            cfg["instance_count"], cfg["unique_user_dir"], cfg["runtime_args"],
            cfg["window_width"], cfg["window_height"], cfg["debugging"], blocking)

    def reload_scene(self) -> bool:
        """Reload the current scene in every instance (hot reload of the scene)."""
        if self._broadcast(runtime_ipc.CMD_RELOAD_SCENE):
            print("Reloading scene...")
            return True
        print("No runtime to reload scene")
        return False

    def load_scene(self, scene_path: str) -> bool:
        """Load a different scene in every running instance."""
        if self._broadcast(runtime_ipc.CMD_LOAD_SCENE, path=str(scene_path)):
            print(f"Loading scene: {scene_path}")
            return True
        print("No runtime to load scene into")
        return False

    def reload_scripts(self) -> bool:
        """Hot-reload script sources in every instance (preserving state where
        the runtime supports it; otherwise a scene reload)."""
        if self._broadcast(runtime_ipc.CMD_RELOAD_SCRIPTS):
            print("Reloading scripts...")
            return True
        print("No runtime to reload scripts")
        return False

    # ------------------------------------------------------------------
    # Live editing (auto hot-reload of editor edits into running instances)
    # ------------------------------------------------------------------

    def _queue_edit(self, key: Tuple, edit: Dict[str, Any]) -> None:
        """Queue one coalesced live edit, keyed so a newer edit to the same
        target replaces an older pending one. No-op when nothing is running."""
        if self._server is None or not self._instances:
            return
        with self._pending_edits_lock:
            # Re-insert at the end so the latest edit to a target wins and edits
            # are flushed roughly in the order they were last touched.
            if key in self._pending_edits:
                del self._pending_edits[key]
            self._pending_edits[key] = edit
        self._ensure_edit_flush_timer()

    def _ensure_edit_flush_timer(self) -> None:
        if self._edit_flush_timer is None:
            self._edit_flush_timer = QTimer()
            self._edit_flush_timer.timeout.connect(self._flush_live_edits)
            self._edit_flush_timer.start(40)

    def _stop_edit_flush_timer(self) -> None:
        if self._edit_flush_timer is not None:
            self._edit_flush_timer.stop()
            self._edit_flush_timer = None

    def _flush_live_edits(self) -> None:
        """Broadcast all pending coalesced edits to every instance as one batch."""
        with self._pending_edits_lock:
            if not self._pending_edits:
                return
            edits = list(self._pending_edits.values())
            self._pending_edits.clear()
        if self._server is None:
            return
        self._broadcast(runtime_ipc.CMD_LIVE_EDIT, edits=json.dumps(edits))

    def push_node_property(self, node_uuid: str, name: str, value: Any) -> None:
        """Push a node property change (e.g. transform) to running instances."""
        if not node_uuid or not name:
            return
        self._queue_edit(("node", node_uuid, name),
                         {"kind": "node", "node": node_uuid, "name": name, "value": value})

    def push_component_property(self, node_uuid: str, index: int, name: str,
                                value: Any) -> None:
        """Push a component property change to running instances. `index` is the
        component's position within its owner node's component list (identical in
        the editor and the runtime since both load the same scene)."""
        if not node_uuid or index < 0 or not name:
            return
        self._queue_edit(("comp", node_uuid, index, name),
                         {"kind": "component", "node": node_uuid, "index": int(index),
                          "name": name, "value": value})

    def push_node_rename(self, node_uuid: str, new_name: str) -> None:
        """Push a node rename to running instances."""
        if not node_uuid:
            return
        self._queue_edit(("rename", node_uuid),
                         {"kind": "rename", "node": node_uuid, "value": new_name})

    def push_asset_reload(self, asset_path: str) -> None:
        """Push an asset file change to running instances so they reload it."""
        if not asset_path:
            return
        self._queue_edit(("asset", asset_path),
                         {"kind": "asset", "path": asset_path})

    def push_reload_scripts(self) -> None:
        """Push a script hot-reload to running instances (coalesced)."""
        self._queue_edit(("scripts",), {"kind": "scripts"})

    # ------------------------------------------------------------------
    # Status
    # ------------------------------------------------------------------

    def is_running(self) -> bool:
        """True while any spawned instance process is still alive."""
        self._instances = [rec for rec in self._instances
                           if rec["proc"].poll() is None]
        return len(self._instances) > 0

    def is_paused(self) -> bool:
        """True when the running instances are paused (aggregate)."""
        if not self._instances:
            return False
        return self._paused

    def get_status(self) -> Dict[str, Any]:
        """Return a snapshot of the current play session."""
        running = self.is_running()
        if not running:
            return {
                "active": False,
                "running": False,
                "paused": False,
                "instances": 0,
                "config": None,
            }

        cfg = self._last_config or {}
        return {
            "active": True,
            "running": True,
            "paused": self._paused,
            "instances": len(self._instances),
            "config": {
                "title": cfg.get("title"),
                "project_path": cfg.get("project_path"),
                "scene_path": cfg.get("scene_path"),
            },
        }

    def cleanup(self) -> None:
        """Full cleanup of runtime resources (call on editor close)."""
        self._teardown_session()
        self._last_config = None
        print("RuntimeController fully cleaned up")

    def __del__(self):
        """Cleanup on deletion."""
        try:
            self.cleanup()
        except Exception:
            pass


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


def step() -> None:
    """Advance one frame using the global controller."""
    get_controller().step()


def relaunch(**kwargs) -> bool:
    """Relaunch the runtime using the global controller."""
    return get_controller().relaunch(**kwargs)


def reload_scene() -> bool:
    """Reload the current scene using the global controller."""
    return get_controller().reload_scene()


def reload_scripts() -> bool:
    """Hot-reload scripts using the global controller."""
    return get_controller().reload_scripts()


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
    print("Runtime controller module loaded successfully!")
    print("Import this module in your editor to control the game runtime.")
