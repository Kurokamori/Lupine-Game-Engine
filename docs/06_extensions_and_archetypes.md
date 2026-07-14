# 06 — Extensions, Custom Components & Archetypes

Three ways to add to the engine: **native C++ extensions** (DLL plugins, no engine rebuild), **built-in C++ components** (recompile core), and **archetypes** (data classes / "ScriptableObject" equivalent).

---

## Native C++ extensions (`.lupineext`)

GDExtension-equivalent: a separate compiled DLL/.so/.dylib loaded at runtime from a manifest. No engine rebuild. SDK in `extension/sdk/include/lupine/ext/` (`Component.hpp`, `Extension.hpp`); C ABI in `extension/include/lupine/lupine_extension_interface.h`.

### Manifest (`name.lupineext`)
```json
{
  "lupine_extension": "1.0",
  "name": "hello_component",
  "description": "…",
  "entry_symbol": "lupine_extension_init",
  "binaries": {
    "windows.x86_64": "bin/hello_component.dll",
    "linux.x86_64":   "bin/hello_component.so",
    "macos.x86_64":   "bin/hello_component.dylib",
    "macos.arm64":    "bin/hello_component.dylib"
  }
}
```
Drop the manifest + `bin/` in the project; `ExtensionManager` loads it and registers the components. Self-contained, portable per directory.

### Component source (full author surface)
```cpp
#include <lupine/ext/Extension.hpp>

class OrbitMover : public lupine::ext::Component {
public:
    void DefineProperties() override {
        ExportFloatRange("angular_speed", 1.5f, -10.0f, 10.0f, 0.05f, "Orbit");
        ExportBool("log_each_second", true, "Debug");
        ExportColor("tint", 1.0f, 0.6f, 0.2f, 1.0f, "Debug");
    }
    void OnReady() override {
        lupine::ext::NodeRef owner = GetOwnerNode();
        LogInfo("ready on " + owner.GetName());
    }
    void OnUpdate(float dt) override {
        lupine::ext::NodeRef owner = GetOwnerNode();
        if (owner.IsValid())
            owner.SetProperty("position", { {"x", x}, {"y", y} });
    }
    void OnDestroy() override {}
    // Reachable from scripts via ComponentRef:call("get_angle")
    nlohmann::json CallMethod(const std::string& m, const nlohmann::json& args) override {
        if (m == "get_angle") return m_angle;
        return nlohmann::json();
    }
private:
    float m_angle = 0.0f;
};

LUPINE_REGISTER_COMPONENT_EX(OrbitMover, "OrbitMover", "" /*base*/, "Native/Examples", LUPINE_HOOKS_ALL)
LUPINE_EXTENSION_MAIN()
```

- **Export helpers** (in `DefineProperties`): `ExportFloat`, `ExportFloatRange`, `ExportInt`, `ExportIntRange`, `ExportBool`, `ExportString`, `ExportColor`, `ExportEnum`, … (last arg is the inspector group).
- **Read values**: `GetFloat/GetInt/GetBool/GetString(name)`; the values live in the engine.
- **Host access**: `GetOwnerNode()` → `lupine::ext::NodeRef` (`IsValid`, `GetName`, `GetProperty`, `SetProperty`, tree ops), `LogInfo/LogWarning/LogError`.
- **Lifecycle**: `OnReady`, `OnUpdate(dt)`, `OnPhysicsUpdate`, `OnDestroy`, etc. `LUPINE_HOOKS_ALL` registers them all (or compose specific hook flags).
- **Scriptable methods**: override `CallMethod` — callable from Lua/Ruby/Python via `ComponentRef:call(name, args)`.
- **Rendering** (ABI v2): override `BuildDrawCommands(RenderContext&)` to record runtime-visible geometry (`DrawQuad/DrawTexturedQuad/DrawRect/DrawRoundedRect/DrawSprite/DrawLine/DrawCircle/DrawPolygon/DrawBox`) and optionally `GetRenderBounds(min,max)` for culling; register with `LUPINE_REGISTER_COMPONENT_DRAWABLE(Class)` (or add `LUPINE_HOOK_DRAW`) so the host gathers it. Editor-only visualization: call `lupine::ext::DebugDraw::Line/Box/Sphere/Circle/Text` (gated on `DebugDraw::Available()`) from `OnRender`.
- **Registration**: `LUPINE_REGISTER_COMPONENT_EX(Class, "TypeName", "BaseType", "Editor/Category", hooks)` then `LUPINE_EXTENSION_MAIN()` once.

### Build
```cmake
lupine_add_extension(hello_component SOURCES src/OrbitMover.cpp)
```
Auto-detects platform, compiles to the right binary, stages into `bin/`.

---

## Built-in C++ components (recompile core)

For engine-level components. Two files: `core/include/lupine/components/Foo.hpp` + `core/src/components/Foo.cpp`. Subclass `core::Component` (or implement `IRenderableComponent` for rendering). Declare properties in `DefineProperties()` with the property macros (see `07_reference.md`).

**Registration sites** (a new built-in must be added to all of these — see `docs/old/COMPONENT_DEVELOPMENT_GUIDE.md`):
1. Add header to the `ComponentRegistry` cluster header.
2. Register the type in **core**.
3. Register in **components** registration.
4. Editor bridge: `Using`, `GetSubcategoryForBaseComponent`, `RegisterType`.
5. Each scripting environment (Lua / MRuby / MicroPython) so it can be inherited from.

Prefer a **native extension** unless the component truly belongs in the engine core.

---

## Archetypes (data classes)

Lupine's ScriptableObject/Resource equivalent: typed data assets, not scene nodes. Files: `core/include/lupine/core/{ArchetypeDefinition,ArchetypeRegistry}.hpp`. Asset extensions `.archetype` (definition) / `.ares` (instance).

### Data-defined (`.archetype` JSON)
```json
{
  "lupine_archetype": 1,
  "archetype_class": "Creature",
  "extends": "GameResource",
  "abstract": false,
  "menu_path": "Gameplay/Creatures",
  "description": "A living creature",
  "fields": [
    { "name": "max_health", "type": 1, "default": 100.0,
      "hint": { "type": 1, "hint_string": "0,1000,1" }, "group": "Combat" },
    { "name": "element", "type": 10, "default": 0,
      "hint": { "type": 2, "hint_string": "fire,ice,poison" } }
  ]
}
```
`type` is a `PropertyValueType` index (see `07_reference.md`); `hint.type` is a `PropertyHintType`. Fields may also carry the extended editor metadata `usage`/`header`/`suffix`/`custom_widget`/`custom_widget_config`/`object_type`/`object_schema` (same as component properties). Archetypes inherit fields from `extends`. Add `"implements": ["Damageable"]` (data) or `--@archetype_implements "Damageable"` (script) to declare the [interfaces](18_interfaces.md) an archetype satisfies; query with `archetype_implements_interface(class, name)` / `get_archetypes_with_interface(name)`.

Archetype field parsing is the **same** shared parser as component script `@export` (see `04_scripting.md`), so the full attribute vocabulary — `[Range]`/`[Header]`/`[Tooltip]`/`[HideInInspector]`/`[ReadOnly]`/`[Required]`/`[Unique]`/`[Custom(...)]`/`@struct`/typed references — works in script-defined archetypes too. `[Required]` and `[Unique]` drive instance-editor validation (empty-required banner; duplicate-value warning across instances of the class). The definition dialog edits these via the Header/Suffix/Usage/Widget columns.

### Asset reference fields (drag-and-drop, multi-value, image preview)
Fields that reference assets get Unity-style **ObjectField** editors. Assets can be dragged from the **file browser** (or dropped from the OS) onto the field; image references show a **collapsible inline preview**; audio references get a **play** button.

Pick an editor with the field's **Widget** column (`custom_widget`) in the definition dialog, or `[Custom("…")]` in a script export:

| Widget id | Field shape | Use |
|---|---|---|
| `resource` / `resource_list` | `Resource` / `StringArray` | Any asset reference, single / multiple |
| `image` / `image_list` | `Resource` / `StringArray` | Image reference(s) with inline preview |
| `audio` / `audio_list` | `Resource` / `StringArray` | Audio reference(s) with a play button |
| `archetype` / `archetype_list` | `Resource` / `StringArray` | `.ares` reference(s); set `custom_widget_config` `{"class":"EnemyStats"}` to filter |

Use a `*_list` widget on a **`StringArray`** field to accept **multiple values** (e.g. several sprites): the list stores a JSON array of `res://` paths, with add / remove / reorder controls and per-element previews. Optional config keys: `extensions` (e.g. `[".png",".jpg"]`), `class` (archetype class to filter), `preview` (bool). A `Resource` field with hint `File`/`ArchetypeType` auto-selects the right slot even without a widget id, and any path/image/audio property on a normal component is drag-droppable too.

### Script-defined archetype
A script can declare an archetype with methods (from `resources/Weapon.lua`):
```lua
--@archetype_class "Weapon"
--@archetype_extends "Item"
--@archetype_menu "Gameplay/Items"
--@archetype_description "A weapon."
--@export damage       float 10.0 @range "0,1000,0.5" @group "Combat"
--@export attack_speed float 1.0  @range "0,20,0.1"   @group "Combat"

-- Methods receive `self` (resolved field values) + any call args; return value goes back to the caller.
function get_dps(self)
    return self.damage * self.attack_speed
end
```

### Runtime read API
Read archetype instances from C++/Lua/MRuby/MicroPython, and call script-defined archetype methods (e.g. `Lupine.call_archetype(...)`). Instances can also be loaded **asynchronously with priority streaming** (`load_archetype_async(path, cb, priority)` / `poll` / `await_archetype`, plus `set_archetype_load_priority` / `set_archetype_streaming_budget`; C-API `asset/lc_async_asset.h`) — see `15_assets_save_localization.md`. Archetypes are data assets in `ArchetypeRegistry`, distinct from `TypeRegistry` node/component types.
