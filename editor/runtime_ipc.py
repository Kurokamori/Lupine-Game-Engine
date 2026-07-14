"""
Lupine Editor <-> Runtime IPC

A small, dependency-free inter-process control channel used by the editor to
drive game instances that now always run in their own OS process. Every "Play"
launch spawns a runtime_instance_runner.py child; this module is how the editor
sends control commands (pause / resume / step / stop / reload-scene /
load-scene / reload-scripts) to those children and how each child streams its
log lines and status back to the editor's console and toolbar.

Design
------
- Transport is a localhost TCP stream. The editor is the server (IpcServer) and
  binds an ephemeral port on 127.0.0.1; each child is a client (IpcClient) that
  connects back to that port. A per-session random token authenticates children
  so an unrelated local process cannot attach.
- Messages are length-prefixed JSON objects: a 4-byte big-endian unsigned length
  followed by that many UTF-8 bytes of a single JSON object. This framing is
  self-delimiting over the byte stream and trivial to parse on both ends.
- Editor -> child messages are *commands* and carry a "cmd" key.
  Child -> editor messages are *events* and carry an "event" key.
- The server's socket threads never touch Qt. They append received events to a
  thread-safe inbox; the editor drains that inbox from the Qt main thread on a
  timer (see RuntimeController). Commands are sent directly from the main thread.

This module is intentionally usable from a plain Python process (the child)
with no Qt dependency, so it lives apart from runtime_controller.py.
"""

from __future__ import annotations

import json
import queue
import secrets
import socket
import struct
import threading
from typing import Any, Callable, Dict, List, Optional, Tuple


# Wire framing -----------------------------------------------------------------

_LENGTH_PREFIX = struct.Struct(">I")
# Guard against a corrupt/hostile length prefix allocating unbounded memory.
_MAX_MESSAGE_BYTES = 64 * 1024 * 1024

# Command names (editor -> child). Kept as constants so both ends agree.
CMD_PAUSE = "pause"
CMD_RESUME = "resume"
CMD_STOP = "stop"
CMD_STEP = "step"
CMD_RELOAD_SCENE = "reload_scene"
CMD_LOAD_SCENE = "load_scene"
CMD_RELOAD_SCRIPTS = "reload_scripts"
CMD_LIVE_EDIT = "live_edit"
CMD_SET_TIME_SCALE = "set_time_scale"
CMD_PING = "ping"

# Event names (child -> editor).
EVENT_HELLO = "hello"
EVENT_READY = "ready"
EVENT_LOG = "log"
EVENT_STATUS = "status"
EVENT_SCENE = "scene"
EVENT_PONG = "pong"
EVENT_CLOSED = "closed"
EVENT_ERROR = "error"


def _send_framed(sock: socket.socket, lock: threading.Lock, obj: Dict[str, Any]) -> bool:
    """Serialize obj as a length-prefixed JSON frame and send it atomically.

    Returns True on success, False if the socket is broken. A per-socket lock
    serializes concurrent senders so frames never interleave on the wire.
    """
    try:
        payload = json.dumps(obj, separators=(",", ":")).encode("utf-8")
    except (TypeError, ValueError):
        return False
    frame = _LENGTH_PREFIX.pack(len(payload)) + payload
    with lock:
        try:
            sock.sendall(frame)
            return True
        except OSError:
            return False


def _recv_exact(sock: socket.socket, count: int) -> Optional[bytes]:
    """Read exactly count bytes, looping over partial recv()s. None on EOF."""
    chunks: List[bytes] = []
    remaining = count
    while remaining > 0:
        try:
            chunk = sock.recv(remaining)
        except OSError:
            return None
        if not chunk:
            return None
        chunks.append(chunk)
        remaining -= len(chunk)
    return b"".join(chunks)


def _recv_framed(sock: socket.socket) -> Optional[Dict[str, Any]]:
    """Read one length-prefixed JSON frame. None on EOF or protocol error."""
    header = _recv_exact(sock, _LENGTH_PREFIX.size)
    if header is None:
        return None
    (length,) = _LENGTH_PREFIX.unpack(header)
    if length == 0 or length > _MAX_MESSAGE_BYTES:
        return None
    body = _recv_exact(sock, length)
    if body is None:
        return None
    try:
        obj = json.loads(body.decode("utf-8"))
    except (ValueError, UnicodeDecodeError):
        return None
    return obj if isinstance(obj, dict) else None


# Editor side ------------------------------------------------------------------

class _Connection:
    """One accepted child connection on the editor side."""

    def __init__(self, instance: int, sock: socket.socket):
        self.instance = instance
        self.sock = sock
        self.send_lock = threading.Lock()
        self.alive = True

    def send(self, obj: Dict[str, Any]) -> bool:
        if not self.alive:
            return False
        ok = _send_framed(self.sock, self.send_lock, obj)
        if not ok:
            self.alive = False
        return ok

    def close(self) -> None:
        self.alive = False
        try:
            self.sock.shutdown(socket.SHUT_RDWR)
        except OSError:
            pass
        try:
            self.sock.close()
        except OSError:
            pass


class IpcServer:
    """Editor-side control server. One per play session.

    Listens on an ephemeral localhost port, accepts one connection per spawned
    instance, and exposes:
      - port / token: hand these to each child so it can connect + authenticate.
      - send(instance, ...) / broadcast(...): issue commands to children.
      - drain_events(): pull received (instance, event_dict) pairs; call this
        from the Qt main thread on a timer.
    """

    def __init__(self, host: str = "127.0.0.1"):
        self._host = host
        self._token = secrets.token_hex(16)
        self._listen = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._listen.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self._listen.bind((host, 0))
        self._listen.listen(16)
        self._port = self._listen.getsockname()[1]

        self._connections: Dict[int, _Connection] = {}
        self._conn_lock = threading.Lock()
        self._inbox: List[Tuple[int, Dict[str, Any]]] = []
        self._inbox_lock = threading.Lock()
        self._closed = False

        self._accept_thread = threading.Thread(
            target=self._accept_loop, name="lupine-ipc-accept", daemon=True)
        self._accept_thread.start()

    # -- properties handed to children -------------------------------------

    @property
    def host(self) -> str:
        return self._host

    @property
    def port(self) -> int:
        return self._port

    @property
    def token(self) -> str:
        return self._token

    # -- accept / read threads ---------------------------------------------

    def _accept_loop(self) -> None:
        while not self._closed:
            try:
                client, _addr = self._listen.accept()
            except OSError:
                return  # listen socket closed during shutdown
            client.settimeout(5.0)
            try:
                hello = _recv_framed(client)
            except OSError:
                hello = None
            if (not hello
                    or hello.get("event") != EVENT_HELLO
                    or hello.get("token") != self._token):
                try:
                    client.close()
                except OSError:
                    pass
                continue
            client.settimeout(None)
            instance = int(hello.get("instance", -1))
            conn = _Connection(instance, client)
            with self._conn_lock:
                old = self._connections.get(instance)
                self._connections[instance] = conn
            if old is not None:
                old.close()
            self._post_event(instance, hello)
            reader = threading.Thread(
                target=self._read_loop, args=(conn,),
                name=f"lupine-ipc-read-{instance}", daemon=True)
            reader.start()

    def _read_loop(self, conn: _Connection) -> None:
        while not self._closed:
            msg = _recv_framed(conn.sock)
            if msg is None:
                break
            self._post_event(conn.instance, msg)
        conn.alive = False
        with self._conn_lock:
            if self._connections.get(conn.instance) is conn:
                del self._connections[conn.instance]
        self._post_event(conn.instance, {"event": EVENT_CLOSED})
        try:
            conn.sock.close()
        except OSError:
            pass

    def _post_event(self, instance: int, msg: Dict[str, Any]) -> None:
        with self._inbox_lock:
            self._inbox.append((instance, msg))

    # -- editor-facing API --------------------------------------------------

    def drain_events(self) -> List[Tuple[int, Dict[str, Any]]]:
        """Return and clear all buffered (instance, event) pairs."""
        with self._inbox_lock:
            if not self._inbox:
                return []
            events = self._inbox
            self._inbox = []
        return events

    def connected_instances(self) -> List[int]:
        with self._conn_lock:
            return sorted(self._connections.keys())

    def send(self, instance: int, command: str, **fields: Any) -> bool:
        """Send one command to a specific instance. False if not connected."""
        with self._conn_lock:
            conn = self._connections.get(instance)
        if conn is None:
            return False
        payload = {"cmd": command}
        payload.update(fields)
        return conn.send(payload)

    def broadcast(self, command: str, **fields: Any) -> int:
        """Send one command to every connected instance. Returns the count sent."""
        with self._conn_lock:
            conns = list(self._connections.values())
        payload = {"cmd": command}
        payload.update(fields)
        sent = 0
        for conn in conns:
            if conn.send(dict(payload)):
                sent += 1
        return sent

    def close(self) -> None:
        """Stop accepting, drop all connections, and free the listen socket."""
        if self._closed:
            return
        self._closed = True
        try:
            self._listen.close()
        except OSError:
            pass
        with self._conn_lock:
            conns = list(self._connections.values())
            self._connections.clear()
        for conn in conns:
            conn.close()


# Child side -------------------------------------------------------------------

class IpcClient:
    """Child-side control client used by runtime_instance_runner.py.

    Connects back to the editor's IpcServer, authenticates with the shared
    token, then:
      - drains inbound commands via poll_commands() from the child's main loop.
      - streams log / status / lifecycle events back with the helper methods.

    A background reader thread fills a thread-safe queue with inbound commands so
    the main loop never blocks on the socket. All sends are serialized by a lock.
    """

    def __init__(self, host: str, port: int, token: str, instance: int,
                 pid: int):
        self._sock = socket.create_connection((host, port), timeout=10.0)
        self._sock.settimeout(None)
        self._send_lock = threading.Lock()
        self._commands: "queue.Queue[Dict[str, Any]]" = queue.Queue()
        self._instance = instance
        self._closed = False

        # Handshake: the server validates token before registering us.
        _send_framed(self._sock, self._send_lock, {
            "event": EVENT_HELLO,
            "token": token,
            "instance": instance,
            "pid": pid,
        })

        self._reader = threading.Thread(
            target=self._read_loop, name="lupine-ipc-client-read", daemon=True)
        self._reader.start()

    @classmethod
    def try_connect(cls, host: str, port: int, token: str, instance: int,
                    pid: int) -> Optional["IpcClient"]:
        """Connect, returning None instead of raising if the editor is gone."""
        try:
            return cls(host, port, token, instance, pid)
        except OSError:
            return None

    def _read_loop(self) -> None:
        while not self._closed:
            msg = _recv_framed(self._sock)
            if msg is None:
                break
            if "cmd" in msg:
                self._commands.put(msg)
        # Signal the main loop that the editor link dropped; it decides policy.
        self._commands.put({"cmd": CMD_STOP, "reason": "ipc-disconnected"})

    # -- main-loop facing ---------------------------------------------------

    def poll_commands(self) -> List[Dict[str, Any]]:
        """Drain all queued inbound commands without blocking."""
        drained: List[Dict[str, Any]] = []
        while True:
            try:
                drained.append(self._commands.get_nowait())
            except queue.Empty:
                break
        return drained

    # -- outbound events ----------------------------------------------------

    def send_event(self, event: str, **fields: Any) -> bool:
        payload = {"event": event, "instance": self._instance}
        payload.update(fields)
        return _send_framed(self._sock, self._send_lock, payload)

    def log(self, level: str, message: str) -> bool:
        return self.send_event(EVENT_LOG, level=level, message=message)

    def status(self, running: bool, paused: bool) -> bool:
        return self.send_event(EVENT_STATUS, running=running, paused=paused)

    def ready(self) -> bool:
        return self.send_event(EVENT_READY)

    def scene(self, path: str) -> bool:
        return self.send_event(EVENT_SCENE, path=path)

    def pong(self, token: Any) -> bool:
        return self.send_event(EVENT_PONG, token=token)

    def closed(self, reason: str = "") -> bool:
        return self.send_event(EVENT_CLOSED, reason=reason)

    def close(self) -> None:
        if self._closed:
            return
        self._closed = True
        try:
            self._sock.shutdown(socket.SHUT_RDWR)
        except OSError:
            pass
        try:
            self._sock.close()
        except OSError:
            pass
