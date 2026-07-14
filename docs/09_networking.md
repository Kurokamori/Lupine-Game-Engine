# 09 — Networking & Multiplayer

Opt-in layer: RPC, state replication with interpolation, spawn/despawn replication, per-node authority — over pluggable transports (ENet UDP, WebSocket, in-process loopback). Gated by `LUPINE_ENABLE_NETWORKING` (ON by default; compiles out cleanly). Full reference: `docs/old/NETWORKING.md`. Scripting module is `Network` (Lua/MRuby) / `network` (MicroPython).

## Offline-safe by design

- No session active → subsystem is one cheap branch per frame.
- `is_multiplayer_authority()` returns **true** offline → authority-gated logic runs unchanged in single-player.
- `rpc(...)` **degrades to a normal local call** when offline.

## Concepts

| Term | Meaning |
|---|---|
| **PeerId** | Participant id. Server/host = `1`; clients = `2,3,…`; `0` = none/broadcast. |
| **NetworkId** | Network-stable id of a replicated node (≠ UUID). Scene nodes get deterministic ids; spawned nodes get high ids from the authority. |
| **Authority** | Peer allowed to drive an object. Server/host owns everything; client-owned setups own their objects. |
| **Session mode** | `Offline`, `Server`, `Host` (listen server + plays), `Client`, `P2P`. |

Any node sending RPCs or being replicated needs a **`NetworkObject`** component (carries NetworkId + authority).

## Session API (`Network.*` / `network.*`)

`start_server(port[,max])`, `start_host(port[,max])`, `connect(address, port)`, `disconnect()`, `is_server()`, `is_client()`, `is_active()`, `get_local_peer_id()`, `get_peer_count()`, `get_peers()`, `connect_signal(event, node, method)`.

Diagnostics: `get_stats()`, `get_peer_loss(peer)`, `get_peer_jitter(peer)`, `get_peer_rtt(peer)`, `kick_peer(peer)`.

Signals: `peer_connected(peer)`, `peer_disconnected(peer)`, `connected_to_server()`, `connection_failed(reason)`, `server_disconnected()`, `reconnecting(attempt)`, `reconnected()`.

```lua
Network.start_host(7777)                 -- host + local player
-- Network.connect("127.0.0.1", 7777)    -- or join
Network.connect_signal("peer_connected", self, "on_peer_joined")
function on_peer_joined(peer_id) Lupine.log_info("peer "..peer_id.." joined") end
```

## RPC

Declare with `--@rpc` (Lua) / `#@rpc` (Python/Ruby) before a function; call through any node handle with a `NetworkObject`.

```lua
--@rpc any_peer reliable call_local
function take_damage(amount)
    health = health - amount
end

some_node:rpc("take_damage", 25)        -- all peers (+ local, since call_local)
some_node:rpc_id(2, "take_damage", 25)  -- one peer
some_node:rpc_unreliable("play_fx")     -- unreliable channel
```

`--@rpc` options (any order): mode `authority` (default) | `any_peer`; transfer `reliable` (default) | `unreliable`; `call_local` (also run on caller). Authority-only RPCs from non-owners are rejected server-side (anti-cheat gate).

Node-handle RPC methods: `rpc(m,...)`, `rpc_id(peer,m,...)`, `rpc_unreliable(m,...)`, `set_multiplayer_authority(peer)`, `get_multiplayer_authority()`, `is_multiplayer_authority()`, `get_network_id()`.

## State replication

Add a **`NetworkSynchronizer`**; list `replicatedProperties` as `"ComponentType:property"` or just `"property"` (first component declaring it — covers script `@export` vars). Only the authority's changed values are sent and applied on others.

```
replicatedProperties = [ "Health:current", "ammo", "PlayerState:team" ]
```

## Transform replication

Add **`NetworkTransform2D`** / **`NetworkTransform3D`**. Authority's transform is replicated and **interpolated** on receivers. Tune `syncPosition/syncRotation/syncScale`, `snapThreshold` (teleport vs slide on big jumps).

## Spawning networked objects

Add a **`NetworkSpawner`**, fill `spawnableScenes` (whitelist of `.scene` paths). On the authority:

```lua
local spawner = Lupine.get_node("%Spawner"):get_component("NetworkSpawner")
local net_id = spawner:call("spawn", "res://prefabs/bullet.scene", Network.get_local_peer_id())
spawner:call("despawn", net_id)
```
Every peer instantiates the same scene with the same NetworkId; the spawnable root should carry a `NetworkObject` (spawner attaches one automatically).

## Client prediction & reconciliation

Add a **`NetworkController`** to a server-authoritative player object (alongside its `NetworkObject` + `NetworkTransform`). Declare its input schema with `axes` / `buttons` (names that double as input-map action/axis names when `autoSampleInput` is on), and on the **server** assign the controlling client:

```lua
player:get_component("NetworkController"):call("set_controller", peer_id)
```

Implement the deterministic step on a script component of the same node:

```lua
-- Runs on the server (authoritative) and is replayed on the controlling client.
function on_network_simulate(input, dt)
    local mx = input["move_x"] or 0
    self:set_position(self:get_position() + Vec2(mx * 200 * dt, 0))
end
```

Each fixed tick the controlling client samples its input, **predicts** locally, and sends the command (with redundancy) to the server; the server applies commands in order and stamps the last-processed input sequence into its snapshots; the client **reconciles** by rewinding to the authoritative state and replaying still-unacknowledged commands — so the owning player feels zero input lag while the server stays authoritative. The host's own player is simulated directly (already authoritative). Toggle globally with `NetworkConfig.enablePrediction`; tune resends with `inputRedundancy`.

`NetworkController` methods (via `get_component:call`): `set_controller(peer)`, `get_controller()`, `set_axis(name,v)`, `get_axis(name)`, `set_button(name,bool)`, `get_button(name)`, `is_predicting_locally()`.

## Physics & animation replication

- **`NetworkRigidBody2D`** / **`NetworkRigidBody3D`** — replicate a sibling `RigidBody2DComponent`/`RigidBody3DComponent` (position, rotation, linear + angular velocity). On non-authority peers the body becomes a kinematic proxy driven by the interpolated stream so the local solver never fights replication, while still pushing the receiver's own dynamic bodies. Props: `syncVelocity`, `snapThreshold`, `compress`, `makeKinematicOnRemote`.
- **`NetworkAnimator`** — replicate an `AnimationPlayer` (clip, time, speed, play/pause) and/or `AnimationTree` (`treeParameters` as `"float:Speed"` etc., plus optional state-machine state). Receivers play locally so animation advances on its own; only changes + periodic time resyncs cross the wire. Props: `syncAnimationPlayer`, `syncAnimationTree`, `syncStateMachine`, `stateLayers`, `treeParameters`, `targetPath`.

## Bandwidth compression

`NetworkTransform2D/3D` and `NetworkRigidBody2D/3D` accept a `compress` flag: positions/scales/velocities ride as 16-bit half floats, 2D rotation as a 16-bit quantized angle, and 3D rotation as a 4-byte smallest-three quaternion — roughly halving each update. The format is flagged per update, so a sender can toggle it freely. Per-property send-rate caps on `NetworkSynchronizer` use the `"entry@hz"` syntax (e.g. `"Health:current@2"` ⇒ at most twice a second).

## Diagnostics

Per-peer bandwidth, packet-loss (from snapshot-sequence gaps), and jitter are tracked live, plus session-wide aggregates:

```lua
local s = Network.get_stats()   -- table: bytes_in/out_per_sec, average_rtt_ms, packet_loss_percent, snapshots_sent/received, ...
local loss = Network.get_peer_loss(peer_id)
local jitter = Network.get_peer_jitter(peer_id)
```

C-API: `lc_net_get_stats(LCNetStats*)`, `lc_net_peer_loss/jitter/bytes_in_per_sec/bytes_out_per_sec(peer)`.

## Reconnection & robustness

- **Auto-reconnect / session resume** — set `NetworkConfig.autoReconnect` (client) and `resumeTimeoutSeconds` (server). On an unexpected drop the client retries (`reconnectAttempts`, `reconnectDelaySeconds` apart) and reclaims its prior peer id + objects if the server still holds the slot; signals `reconnecting(attempt)` and `reconnected()` report progress.
- **Keyframe-on-loss** — receivers ack the latest snapshot sequence; a server that sees a large gap pushes a recovery keyframe immediately rather than waiting for the periodic one.
- **Anti-flood** — `maxMessagesPerSecond` / `maxBytesPerSecond` force-disconnect a peer that exceeds either inbound budget.

These are configured per session (`NetworkConfig` via scripting / C-API `LCNetConfig`).

## Project settings

`networking` section of `.lupine`: `transport` (`enet`/`websocket`/`loopback`), `default_port`, `max_peers`, `tick_rate`, `protocol_version`, `game_id`.

## Local multi-instance testing (editor)

Every editor "Play" launch now runs the game in its own isolated OS process with its own window, engine singletons (VFS / InputManager / script hosts / graphics device) and main thread — controlled by the editor over an IPC channel (pause / resume / step / stop / reload-scene / reload-scripts), with each instance's log output mirrored into the editor console. See `docs/17_editor_run_ipc.md`.

**Project Settings → Networking → Multi-Instance Testing**: set instance count (1 = one isolated process; 2+ spawns that many), per-instance `user://` dir, and runtime args (`{instance}` → zero-based index). Read args in-game with `get_cmdline_args()` (all 3 langs; C-API `lc_cmdline_arg_count`/`lc_cmdline_arg_at`).

```lua
-- Runtime Args: "--role {instance} --port 7777"
local args = get_cmdline_args()
local role = "0"
for i = 1, #args - 1 do if args[i] == "--role" then role = args[i+1] end end
if role == "0" then Network.start_host(7777) else Network.connect("127.0.0.1", 7777) end
```

## C-API (`capi/include/network/`)

- `lc_network.h` — session control, peer queries, diagnostics (`lc_net_get_stats`, `lc_net_peer_loss/jitter`), signal callbacks, `lc_net_poll`/`lc_net_flush` (hosts without an engine loop). `LCNetConfig` carries the prediction / reconnect / anti-flood fields.
- `lc_rpc.h` — `lc_net_rpc` / `lc_net_rpc_id` / `lc_net_rpc_unreliable` + `lc_net_register_component_rpc`.
- `lc_replication.h` — `lc_net_object_id`, authority get/set, `lc_net_spawn`/`lc_net_despawn`.

Args marshal as JSON-array strings. Example: `capi/examples/suite/test_ext_network.inc`.

## Architecture

`NetworkManager` (singleton) owns the session, holds `INetworkTransport[]` (ENet / WebSocket / Loopback), `RpcManager`, `ReplicationManager`, and `PredictionManager`. `SceneManager::Update` calls `Poll()` before scripts and `Flush()` after; `PhysicsUpdate` calls `GenerateSnapshot()` after the physics solve (which first runs the prediction tick). Wire frame: `[channel][type][payload]`, LEB128 varints, size-bounded; snapshots carry a sequence number for loss/jitter estimation.

## Not yet built (documented)

Browser P2P (WebRTC data channels), relay / matchmaking / NAT traversal. (Client prediction, `NetworkRigidBody`/`NetworkAnimator`, per-property send rates, interest management, bandwidth compression, diagnostics, and reconnection/resume are now implemented — see above.)
