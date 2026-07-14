"""
Lupine Runtime Instance Runner

Standalone entry point the editor spawns for EVERY "Play" launch. Each game
instance now always runs in its own OS process with its own window, its own
engine singletons (VFS / InputManager / script hosts / graphics device) and
(optionally) its own isolated user:// directory. Running one instance or many
is the same mechanism - the editor just spawns one of these per instance.

This deliberately reuses the very same lupine_runtime extension module the
editor itself loads, so an in-editor play and a packaged build behave
identically. The parent passes its resolved sys.path through the
LUPINE_RUNNER_SYSPATH environment variable so the import below resolves to the
exact same build the editor is using.

Control model
-------------
The runtime runs its frame loop on its own thread (app.run_async()); this
process's main thread pumps SDL events (app.process_events(), which SDL requires
on the thread that created the window) and applies control commands the editor
sends over the IPC channel (see runtime_ipc.IpcClient): pause / resume / step /
stop / reload-scene / load-scene / reload-scripts. The same loop streams the
runtime's log lines and status changes back to the editor's console and toolbar.

When no IPC endpoint is supplied (standalone use) the loop still runs; it simply
has no editor to talk to and exits when the window closes.

Invocation (built by RuntimeController._spawn_instance):

    python runtime_instance_runner.py
        --project <path.lupine>
        [--scene <path.scene>]
        [--backend <opengl|vulkan|directx11|directx12|metal|webgl>]
        [--user-path <dir>]
        [--width <px>] [--height <px>]
        [--title <text>]
        [--profiler-dump <path.json>]
        [--ipc-host <ip>] [--ipc-port <port>] [--ipc-token <hex>]
        [--instance <index>]
        [--debug]
        [-- <game args forwarded to get_cmdline_args()>]
"""

import argparse
import os
import sys
import threading
import time

# Resolve runtime_ipc relative to this file regardless of CWD (the editor runs
# each isolated instance from its own user directory).
_THIS_DIR = os.path.dirname(os.path.abspath(__file__))
if _THIS_DIR not in sys.path:
    sys.path.insert(0, _THIS_DIR)

import runtime_ipc as ipc_mod


def _bootstrap_module_path() -> None:
    """Replicate the parent editor's import environment before importing lr."""
    raw = os.environ.get("LUPINE_RUNNER_SYSPATH", "")
    if not raw:
        return
    entries = [p for p in raw.split(os.pathsep) if p]
    # Prepend in reverse so the parent's ordering is preserved at the front.
    for entry in reversed(entries):
        if entry not in sys.path:
            sys.path.insert(0, entry)
        # Native extension modules (.pyd) resolve dependent DLLs relative to
        # their own directory on modern Python; register those directories.
        if hasattr(os, "add_dll_directory") and os.path.isdir(entry):
            try:
                os.add_dll_directory(entry)
            except (OSError, FileNotFoundError):
                pass


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Launch a single Lupine runtime instance.")
    parser.add_argument("--project", required=True)
    parser.add_argument("--scene", default="")
    parser.add_argument("--backend", default="")
    parser.add_argument("--user-path", dest="user_path", default="")
    parser.add_argument("--width", type=int, default=1280)
    parser.add_argument("--height", type=int, default=720)
    parser.add_argument("--title", default="")
    parser.add_argument("--profiler-dump", dest="profiler_dump", default="")
    parser.add_argument("--ipc-host", dest="ipc_host", default="127.0.0.1")
    parser.add_argument("--ipc-port", dest="ipc_port", type=int, default=0)
    parser.add_argument("--ipc-token", dest="ipc_token", default="")
    parser.add_argument("--instance", type=int, default=0)
    parser.add_argument("--debug", action="store_true")
    # Everything after a literal "--" is forwarded verbatim to the game.
    parser.add_argument("game_args", nargs="*")
    return parser.parse_args()


def _resolve_backend(lr, le, name: str):
    """Map a backend name string to the enum lupine_runtime expects."""
    if not name:
        return None

    candidates = []
    if le is not None and hasattr(le, "GraphicsBackend"):
        candidates.append(le.GraphicsBackend)
    if hasattr(lr, "GraphicsBackend"):
        candidates.append(lr.GraphicsBackend)

    alias = {
        "opengl": "OpenGL",
        "vulkan": "Vulkan",
        "dx11": "DirectX11",
        "directx11": "DirectX11",
        "dx12": "DirectX12",
        "directx12": "DirectX12",
        "metal": "Metal",
        "webgl": "WebGL",
    }
    enum_name = alias.get(name.lower(), name)
    for enum_type in candidates:
        member = getattr(enum_type, enum_name, None)
        if member is not None:
            return member
    return None


def _start_profiler_dump(lr, dump_path: str) -> None:
    """
    Enable this instance's profiler and stream its history to a JSON file so the
    editor's profiler panel can read it across the process boundary.

    The editor cannot call into a spawned instance's profiling::Profiler singleton
    directly (it lives in a different process), so each instance periodically
    snapshots profiler_get_history_json() and atomically replaces dump_path with
    it. The editor's ProfilerPanel polls that file for this instance's tab.
    """
    if not (hasattr(lr, "profiler_get_history_json") and hasattr(lr, "profiler_set_enabled")):
        return

    try:
        lr.profiler_set_enabled(True)
    except Exception:
        return
    if hasattr(lr, "profiler_set_history_size"):
        try:
            lr.profiler_set_history_size(300)
        except Exception:
            pass

    tmp_path = dump_path + ".tmp"

    def _pump() -> None:
        while True:
            try:
                raw = lr.profiler_get_history_json()
            except Exception:
                raw = ""
            if raw:
                try:
                    with open(tmp_path, "w", encoding="utf-8") as handle:
                        handle.write(raw)
                    os.replace(tmp_path, dump_path)
                except OSError:
                    pass
            time.sleep(0.15)

    worker = threading.Thread(target=_pump, name="lupine-profiler-dump", daemon=True)
    worker.start()


def _connect_ipc(args):
    """Connect back to the editor's IPC server, or None for standalone runs."""
    if not args.ipc_port or not args.ipc_token:
        return None
    client = ipc_mod.IpcClient.try_connect(
        args.ipc_host, args.ipc_port, args.ipc_token, args.instance, os.getpid())
    if client is None:
        sys.stderr.write(
            "runtime_instance_runner: could not reach editor IPC server; "
            "running without remote control.\n")
    return client


def _single_step(app) -> None:
    """Advance exactly one update+physics frame while keeping the runtime paused.

    Uses the native step() primitive when available (precise: forces one update
    on the runtime thread's next frame). Falls back to briefly resuming for one
    frame interval, which is approximate but works on builds without step().
    """
    if hasattr(app, "step"):
        try:
            app.step()
            return
        except Exception:
            pass
    was_paused = app.is_paused()
    app.resume()
    time.sleep(1.0 / 60.0)
    if was_paused:
        app.pause()


def _hot_reload_scripts(app) -> None:
    """Reload script sources in-place, preserving runtime/node state when the
    build supports it; otherwise fall back to a full scene reload."""
    if hasattr(app, "reload_scripts"):
        try:
            app.reload_scripts()
            return
        except Exception:
            pass
    app.reload_scene()


def _apply_live_edits(app, edits_json: str) -> None:
    """Apply a batch of editor live edits to the running scene in place.

    Uses the native apply_live_edits() primitive when available (preserves
    gameplay state - the expected hot-reload behaviour). On older builds that
    lack it, fall back to reloading assets / scripts / scene as best we can so
    edits still become visible, accepting that a scene reload loses state."""
    if hasattr(app, "apply_live_edits"):
        try:
            app.apply_live_edits(edits_json)
            return
        except Exception:
            pass

    # Fallback for builds without the native primitive: approximate by reacting
    # to the kinds of edits present in the batch.
    try:
        import json as _json
        edits = _json.loads(edits_json)
    except (ValueError, TypeError):
        return
    if not isinstance(edits, list):
        return

    needs_scene_reload = False
    for edit in edits:
        if not isinstance(edit, dict):
            continue
        kind = edit.get("kind", "")
        if kind == "scripts":
            _hot_reload_scripts(app)
        elif kind in ("node", "component", "rename"):
            # No in-place setter on this build; a scene reload is the only way to
            # reflect property changes.
            needs_scene_reload = True
        elif kind == "asset":
            needs_scene_reload = True
    if needs_scene_reload:
        app.reload_scene()


def _dispatch_command(app, ipc, command: dict, state: dict) -> None:
    """Apply one editor command to the runtime on this process's main thread."""
    name = command.get("cmd", "")

    if name == ipc_mod.CMD_PAUSE:
        app.pause()
    elif name == ipc_mod.CMD_RESUME:
        app.resume()
    elif name == ipc_mod.CMD_STEP:
        _single_step(app)
    elif name == ipc_mod.CMD_RELOAD_SCENE:
        app.reload_scene()
        if ipc is not None:
            ipc.scene(state.get("scene", ""))
    elif name == ipc_mod.CMD_LOAD_SCENE:
        path = command.get("path", "")
        if path:
            if app.load_scene(str(path)):
                state["scene"] = str(path)
            if ipc is not None:
                ipc.scene(state.get("scene", ""))
    elif name == ipc_mod.CMD_RELOAD_SCRIPTS:
        _hot_reload_scripts(app)
    elif name == ipc_mod.CMD_LIVE_EDIT:
        _apply_live_edits(app, command.get("edits", "[]"))
    elif name == ipc_mod.CMD_SET_TIME_SCALE:
        # Honoured only on builds that expose a time-scale setter; harmless
        # no-op otherwise so older modules keep working.
        if hasattr(app, "set_time_scale"):
            try:
                app.set_time_scale(float(command.get("value", 1.0)))
            except Exception:
                pass
    elif name == ipc_mod.CMD_PING:
        if ipc is not None:
            ipc.pong(command.get("token"))
    elif name == ipc_mod.CMD_STOP:
        state["stop"] = True


def _drain_runtime_logs(lr, ipc) -> None:
    """Forward this instance's buffered runtime log lines to the editor."""
    if ipc is None:
        return
    try:
        entries = lr.drain_log_messages()
    except Exception:
        return
    for entry in entries:
        try:
            level, message = entry
        except (ValueError, TypeError):
            continue
        ipc.log(str(level), str(message))


def _run_control_loop(lr, app, ipc, initial_scene: str) -> None:
    """Pump SDL events + IPC commands on the main thread while the runtime thread
    drives frames. Returns when the window closes, the runtime quits, or the
    editor sends a stop command."""
    state = {"stop": False, "scene": initial_scene}
    last_status = None

    if ipc is not None:
        ipc.ready()

    while True:
        if ipc is not None:
            for command in ipc.poll_commands():
                _dispatch_command(app, ipc, command, state)
                if state["stop"]:
                    break
        if state["stop"]:
            break

        # SDL events must be pumped on the thread that created the window.
        if not app.process_events():
            break
        if not app.is_running():
            break

        _drain_runtime_logs(lr, ipc)

        if ipc is not None:
            status = (app.is_running(), app.is_paused())
            if status != last_status:
                ipc.status(status[0], status[1])
                last_status = status

        # Match the editor's historical 1ms cadence: responsive event handling
        # without busy-spinning the main thread (the runtime thread owns pacing).
        time.sleep(0.001)


def main() -> int:
    _bootstrap_module_path()
    args = _parse_args()

    try:
        import lupine_runtime as lr
    except ImportError as exc:
        sys.stderr.write(f"runtime_instance_runner: lupine_runtime not importable: {exc}\n")
        return 2

    try:
        import lupine_engine as le
    except ImportError:
        le = None

    ipc = _connect_ipc(args)

    config = lr.RuntimeConfig()
    config.title = args.title or f"Lupine Runtime - {os.path.splitext(os.path.basename(args.project))[0]}"
    config.window_width = args.width
    config.window_height = args.height
    config.debugging = bool(args.debug)
    config.project_path = args.project
    config.scene_path = args.scene or ""

    if args.user_path:
        try:
            config.user_path = args.user_path
        except AttributeError:
            pass

    if args.game_args:
        try:
            config.args = list(args.game_args)
        except AttributeError:
            pass

    backend = _resolve_backend(lr, le, args.backend)
    if backend is not None:
        try:
            config.backend = backend
        except (AttributeError, TypeError):
            pass

    app = lr.RuntimeApp()
    if not app.initialize(config):
        sys.stderr.write(f"runtime_instance_runner: failed to initialize runtime for {args.project}\n")
        if ipc is not None:
            ipc.send_event(ipc_mod.EVENT_ERROR, message="initialize failed")
            ipc.closed(reason="initialize-failed")
            ipc.close()
        return 1

    if args.profiler_dump:
        _start_profiler_dump(lr, args.profiler_dump)

    # Runtime owns its own thread; this process's main thread pumps events and
    # applies editor commands. Each instance is its own process, so there is no
    # shared SDL/event state to coordinate with any other instance.
    app.run_async()
    try:
        _run_control_loop(lr, app, ipc, args.scene or "")
    finally:
        app.stop()
        app.shutdown()
        _drain_runtime_logs(lr, ipc)
        if ipc is not None:
            ipc.closed()
            ipc.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
