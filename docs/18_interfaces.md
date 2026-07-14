# 18 — Interface Types (Capability Contracts)

Interfaces are named **capability contracts** — a way to declare that an object is, for example, `Damageable`, and to find every `Damageable` in the scene at runtime or in the editor. Unlike [groups](14_signals_events_groups.md) (which are ad-hoc tags on individual nodes), an interface is declared by a **script**, an **archetype**, or a **native component type**, carries an optional **method + signal contract**, and is verifiable. Interfaces pair directly with the [signal system](14_signals_events_groups.md): an interface's required signals are auto-declared on every implementer, so you can `connect`/`emit` across "all Damageables" with no boilerplate.

Sources: `core/include/lupine/core/{InterfaceDefinition,InterfaceRegistry}.hpp`; conformance on `Node`/`Component`/`ScriptComponent`/`Scene`; scripting in `*Environment.cpp`; C API `capi/include/core/lc_interface.h`; editor `editor/dialogs/interface_definition_dialog.py` + `editor/panels/interface_panel.py`.

## Concept

An interface has a **name**, optional **base interfaces** it extends, a set of **required methods**, a set of **required signals**, a description, and tags. A node *implements* an interface when any of its components or scripts declares it (directly or through interface inheritance). Implementing a derived interface satisfies every base interface in its chain.

```
interface Damageable extends Entity {
    method take_damage(amount)
    method heal(amount)
    signal died
    signal health_changed(hp)
}
```

## Defining an interface

Interfaces come from three places; all merge into one registry (`InterfaceRegistry`). Native and runtime-registered interfaces survive project/scene rescans.

### 1. `.interface` asset file (primary)

Authored in the editor (**Interfaces** panel → *New Interface…*) or by hand. JSON schema:
```json
{
  "lupine_interface": 1,
  "interface_name": "Damageable",
  "description": "Anything that can take damage",
  "extends": ["Entity"],
  "methods": [
    { "name": "take_damage", "params": [{ "name": "amount", "type": 1 }] },
    { "name": "heal", "params": [{ "name": "amount", "type": 1 }] }
  ],
  "signals": [
    { "name": "died", "args": [] },
    { "name": "health_changed", "args": [{ "name": "hp", "type": 1 }] }
  ],
  "tags": ["combat"]
}
```
`type` integers are `PropertyValueType` enum values (see [16](16_component_properties.md)); `1` is `Float`. `methods` and `signals` entries may also be bare name strings when no argument metadata is needed. `extends` may be a single string or an array.

### 2. Inline in a script

A script can fully define an interface with header directives (Lua `--`, Ruby/Python `#`):
```lua
--@interface_define "Damageable"
--@interface_extends "Entity"
--@interface_description "Anything that can take damage"
--@interface_method take_damage(float amount)
--@interface_method heal(float amount)
--@interface_signal died
--@interface_signal health_changed(float hp)
--@interface_tag combat
```

### 3. Native (C++)

Built-in / native-extension interfaces register themselves at startup via `InterfaceRegistry::RegisterNativeInterface(def)`, or from C with `lc_interface_register_json`.

## Implementing an interface

### Scripts — `@interface`

```lua
--@interface Damageable
-- (or several: --@interface Damageable, Destructible)

function take_damage(amount) ... end
function heal(amount) ... end
```
When the script loads, the engine registers the interface's required signals (`died`, `health_changed`) on the component automatically, so handlers can connect to them immediately.

### Archetypes — `implements`

Data archetype (`.archetype`): add an `"implements": ["Damageable"]` array at the root. Script archetype: `--@archetype_implements "Damageable" "Destructible"`. A derived archetype implements the union of its own and its base chain's declarations.

### Native components — `REGISTER_COMPONENT_INTERFACES`

Next to `REGISTER_COMPONENT_TYPE` in the component's `.cpp`:
```cpp
REGISTER_COMPONENT_INTERFACES(HealthComponent, "Damageable", "Destructible")
```
A component may also override `Component::GetImplementedInterfaces()` for per-instance interfaces and `Component::HasInterfaceMethod()` to advertise the methods it answers for contract verification.

## Querying at runtime

A node implements an interface if any of its components declares it (directly or via inheritance). The scene-wide query mirrors `get_nodes_in_group`.

### Scripting (Lua / MRuby / MicroPython)

Global functions (module `lupine` / `Lupine`):

| Function | Result |
|---|---|
| `implements_interface(name)` | does the owner node implement it? |
| `get_implemented_interfaces()` | the owner's interfaces (incl. inherited) |
| `verify_interface(name)` | `{ interface, exists, conforms, missing_methods, missing_signals }` for the owner |
| `get_nodes_with_interface(name)` | every node in the scene implementing it (node handles) |
| `get_node_count_with_interface(name)` | count of the above |
| `get_first_node_with_interface(name)` | first match, or nil/None |
| `interface_exists(name)` | is it a registered interface? |
| `get_all_interfaces()` | all registered interface names |
| `get_interface_definition(name)` | the serialized definition, or nil/None |
| `register_interface(table)` | define an interface at runtime (persists across scenes) |
| `archetype_implements_interface(class, name)` | does an archetype class implement it? |
| `get_archetypes_with_interface(name)` | archetype classes implementing it |

Node-handle methods: `node:implements_interface(name)`, `node:get_interfaces()`, `node:verify_interface(name)`.

```lua
function on_explode()
    for _, n in ipairs(get_nodes_with_interface("Damageable")) do
        n:call("take_damage", 50)            -- drive the contract method
        n:connect("died", self, "on_thing_died")  -- pair with signals
    end
end
```
MRuby and MicroPython expose the identical surface (`Lupine.get_nodes_with_interface("Damageable")`, `lupine.implements_interface("Damageable")`, etc.).

### C++

`Node::ImplementsInterface(name)`, `Node::GetImplementedInterfaces()`, `Node::VerifyInterface(name)`, and `Scene::GetNodesImplementingInterface(name)`. The full API lives on `InterfaceRegistry::GetInstance()` (`GetEffectiveMethods/Signals`, `IsSubInterfaceOf`, `GetTypesImplementing`, `VerifyMembers`, …) and `ArchetypeRegistry` (`ArchetypeImplementsInterface`, `GetArchetypesImplementing`).

## Verification

`verify_interface(name)` (script) / `lc_node_verify_interface_json` (C) / `Node::VerifyInterface` (C++) check a node against the interface's **effective** contract (own + inherited) and return which required methods and signals are missing. A method is "present" if any component answers it (`HasInterfaceMethod` — script functions count); a signal is "present" if the node or any component declares it. The editor's **Interfaces** panel and the inspector surface this as a ✓/✗ conformance marker.

## C API

`capi/include/core/lc_interface.h` mirrors the whole surface: `lc_interface_exists`, `lc_interface_get_count`/`_get_name_at`, `lc_interface_get_definition_json`, `lc_interface_register_json`, `lc_interface_verify_members_json`, `lc_interface_register_type_conformance`, `lc_interface_type_implements`, `lc_interface_get_types_implementing_json`, `lc_node_implements_interface`, `lc_node_get_interfaces_json`, `lc_node_verify_interface_json`, `lc_component_implements_interface`, `lc_component_get_interfaces_json`, `lc_scene_find_nodes_by_interface`, `lc_scene_get_first_node_by_interface`, `lc_archetype_implements_interface`, `lc_archetype_get_interfaces_json`, `lc_archetype_get_implementing_json`. JSON/string out-parameters are owned by the caller (`lc_free`).

## Editor

* **Interfaces panel** — browse all interfaces, see each one's methods/signals/tags and its implementers (nodes in the open scene, archetype classes, native component types). Create/edit/delete `.interface` files.
* **Interface definition dialog** — author the contract: name, description, extends (multi-select), methods table, signals table, tags.
* **Archetype definition dialog** — an *Implements Interfaces* multi-select writes the `implements` array into the archetype schema.
* Definitions hot-reload: editing a `.interface` (or a defining script) rescans the registry.

## Relationship to groups

Use a **group** for an arbitrary, per-instance tag set on specific nodes ("enemies in room 3"). Use an **interface** for a *capability* declared by a script/archetype/component type, especially when you want a verifiable method+signal contract and want every implementer to share signals. They compose freely — query a group, then check `implements_interface` on the results, or vice-versa.

See also: [14 — Signals, Events & Groups](14_signals_events_groups.md), [06 — Extensions & Archetypes](06_extensions_and_archetypes.md), [04 — Scripting](04_scripting.md).
