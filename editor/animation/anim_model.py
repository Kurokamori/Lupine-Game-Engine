"""
Animation data model helpers (.animclip / .animgraph JSON schema).

Mirrors core/include/lupine/animation/AnimationData.hpp. The editor works with plain
dicts that match the on-disk JSON exactly; these helpers centralize construction,
keyframe operations, and channel inference so the timeline and blend-tree editors
share one source of truth.
"""

import json

# Channel vocabulary (must match AnimationChannel / Tween).
TRANSFORM_CHANNELS_2D = ["position_2d", "rotation_2d", "scale_2d"]
TRANSFORM_CHANNELS_3D = ["position_3d", "rotation_3d", "scale_3d"]

VALUE_TYPES = ["float", "vec2", "vec3", "vec4", "color", "bool", "int", "quat"]
INTERP_MODES = ["constant", "linear", "cubic"]
LOOP_MODES = ["none", "loop", "pingpong"]
PARAM_TYPES = ["float", "int", "bool", "trigger"]
CONDITION_OPS = ["greater", "less", "equal", "not_equal", "is_true", "is_false"]
BLEND_NODE_TYPES = ["clip", "blend1d", "blend2d", "direct", "blend2", "add2", "oneshot", "timescale"]
BLEND2D_MODES = ["simple_directional", "freeform_directional", "freeform_cartesian", "direct"]

# Inspector PropertyValueType enum -> animation valueType string. Every animatable
# type maps to a value type; string / node-path / scene-path / enum / resource are
# exposed too (enums animate as ints, the rest as discrete strings). Double animates
# like a float. The container/struct types (StringArray, Quat, Rect, IntArray,
# FloatArray, Array, Dictionary) are not keyframable and are intentionally omitted.
PROPERTY_TYPE_TO_VALUE_TYPE = {
    0: "int", 1: "float", 2: "string", 3: "bool", 4: "vec2", 5: "vec3", 6: "vec4",
    7: "color", 8: "string", 9: "string", 10: "int",
    12: "float", 15: "string",
}


def value_type_for_property(prop_type):
    """Map an inspector PropertyValueType int to an animation valueType (or None)."""
    return PROPERTY_TYPE_TO_VALUE_TYPE.get(prop_type)


def infer_value_type(channel):
    if channel in ("position_2d", "scale_2d", "global_position_2d"):
        return "vec2"
    if channel == "rotation_2d":
        return "float"
    if channel in ("position_3d", "scale_3d", "global_position_3d"):
        return "vec3"
    if channel == "rotation_3d":
        return "quat"
    if channel == "modulate":
        return "color"
    return "float"


def value_component_count(value_type):
    return {"float": 1, "bool": 1, "int": 1, "string": 1, "vec2": 2, "vec3": 3,
            "quat": 3, "vec4": 4, "color": 4}.get(value_type, 1)


def default_value(value_type):
    if value_type == "bool":
        return False
    if value_type == "int":
        return 0
    if value_type == "string":
        return ""
    n = value_component_count(value_type)
    return 0.0 if n == 1 else [0.0] * n


# ---------------------------------------------------------------------------
# Clip
# ---------------------------------------------------------------------------

def new_clip(name="New Clip"):
    return {"version": 1, "name": name, "length": 1.0, "fps": 30, "loop": "none", "tracks": []}


def new_value_track(node, channel, value_type=None, interp="linear", node_uuid=""):
    vt = value_type or infer_value_type(channel)
    track = {"type": "value", "node": node, "channel": channel, "valueType": vt,
             "interp": interp, "useSlerp": channel == "rotation_3d", "keys": []}
    if node_uuid:
        track["nodeUuid"] = node_uuid
    return track


def new_method_track(node, node_uuid=""):
    track = {"type": "method", "node": node, "keys": []}
    if node_uuid:
        track["nodeUuid"] = node_uuid
    return track


def find_track(clip, node, channel):
    for track in clip["tracks"]:
        if track.get("type", "value") == "value" and track["node"] == node and track["channel"] == channel:
            return track
    return None


def track_label(track):
    node = track.get("node", "") or "(root)"
    if track.get("type") == "method":
        return "%s : (method)" % node
    return "%s : %s" % (node, track.get("channel", ""))


def insert_key(track, time, value, interp=None):
    keys = track.setdefault("keys", [])
    for k in keys:
        if abs(k["t"] - time) < 1e-4:
            k["v"] = value
            return k
    key = {"t": float(time), "v": value}
    keys.append(key)
    keys.sort(key=lambda kk: kk["t"])
    return key


def insert_method_key(track, time, method, args=None):
    key = {"t": float(time), "method": method, "args": args or []}
    track.setdefault("keys", []).append(key)
    track["keys"].sort(key=lambda kk: kk["t"])
    return key


def remove_key(track, index):
    keys = track.get("keys", [])
    if 0 <= index < len(keys):
        keys.pop(index)


def clip_targets(clip):
    out = []
    for track in clip["tracks"]:
        if track.get("type", "value") == "value":
            out.append({"node": track["node"], "nodeUuid": track.get("nodeUuid", ""),
                        "channel": track["channel"],
                        "valueType": track.get("valueType", infer_value_type(track["channel"]))})
    return out


def clip_length(clip):
    if clip.get("length", 0) > 0:
        return float(clip["length"])
    longest = 0.0
    for track in clip["tracks"]:
        if track.get("keys"):
            longest = max(longest, track["keys"][-1]["t"])
    return longest


# ---------------------------------------------------------------------------
# Graph
# ---------------------------------------------------------------------------

def new_graph():
    return {"version": 1, "parameters": [], "layers": [new_layer("Base")]}


def new_layer(name="Layer"):
    return {"name": name, "weight": 1.0, "blendMode": "override",
            "mask": {"mode": "include", "targets": []},
            "stateMachine": {"entry": "", "states": [], "transitions": []}}


def new_param(name, ptype="float"):
    param = {"name": name, "type": ptype}
    param["default"] = False if ptype in ("bool", "trigger") else (0 if ptype == "int" else 0.0)
    return param


def new_clip_state(name, clip="", pos=(0, 0)):
    return {"name": name, "type": "clip", "clip": clip, "speed": 1.0, "pos": [pos[0], pos[1]]}


def new_blendtree_state(name, pos=(0, 0)):
    return {"name": name, "type": "blendtree", "speed": 1.0, "pos": [pos[0], pos[1]],
            "blendTree": new_blend_node("clip")}


def new_blend_node(node_type="clip"):
    node = {"type": node_type}
    if node_type == "clip":
        node["clip"] = ""
        node["speed"] = 1.0
    elif node_type in ("blend1d", "blend2", "add2", "timescale"):
        node["parameter"] = ""
        node["children"] = []
    elif node_type == "blend2d":
        node["parameterX"] = ""
        node["parameterY"] = ""
        node["mode"] = "freeform_directional"
        node["children"] = []
    elif node_type == "direct":
        node["children"] = []
    elif node_type == "oneshot":
        node["parameter"] = ""
        node["fadeIn"] = 0.2
        node["fadeOut"] = 0.2
        node["children"] = []
    return node


def new_transition(frm, to):
    return {"from": frm, "to": to, "hasExitTime": False, "exitTime": 1.0,
            "duration": 0.2, "priority": 0, "conditions": []}


def new_condition(param="", op="greater", value=0.0):
    return {"param": param, "op": op, "value": value}


# ---------------------------------------------------------------------------
# File IO
# ---------------------------------------------------------------------------

def load_json(path):
    with open(path, "r", encoding="utf-8") as handle:
        return json.load(handle)


def save_json(path, data):
    with open(path, "w", encoding="utf-8") as handle:
        json.dump(data, handle, indent=2)
