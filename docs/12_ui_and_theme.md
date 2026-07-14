# 12 — UI & Theme

UI components derive from `UIControl` (anchors/offsets/size-flags). Containers auto-layout children. A Theme system styles controls from `.uitheme`/`.palette` assets. Sources: `core/include/lupine/components/UIControl.hpp`, `ui/ThemeManager.hpp`; component list in `03_nodes_and_components.md`.

## The canvas is Y-up (read this first)

The 2D/UI canvas is **Y-up and centered on the world origin**: a W×H canvas spans
`[-W/2, +W/2] × [-H/2, +H/2]`, and **+Y points toward the top of the screen**.

Two consequences that are not guessable and that account for most historical UI layout bugs:

- **`math::Rect::position` is the MINIMUM corner — the BOTTOM-left, not the top-left.**
  Never write `rect.position.y + h` when you mean an edge. Use the named accessors:
  `GetTopEdge()` / `GetBottomEdge()` / `GetLeftEdge()` / `GetRightEdge()`, `GetTopLeft()` and
  friends, or `Rect::FromEdges(left, top, right, bottom)` (which clamps an inverted rect to
  zero extent instead of producing a negative size).
- **Anchors and offsets are screen-oriented, not canvas-oriented.** `anchor.y = 0` is the
  parent's **TOP** edge and `anchor.y = 1` its bottom. A **positive offset moves its edge
  DOWN** the screen, on both axes — so `offsetMin.y` insets from the top and `offsetMax.y`
  from the bottom, exactly mirroring `offsetMin.x` (left) and `offsetMax.x` (right).

## UIControl layout

Every UI control has:

- **`layoutMode`** — `Position` (free: the Node2D position and `width`/`height` place it) or
  `Anchors` (anchors + offsets place it).
- **Anchors** (`anchorMin` / `anchorMax`, 0..1 of the parent rect) and **offsets**
  (`offsetMin` / `offsetMax`, pixels).
- **`anchorPreset`** — the common setups (top-left, center, full-rect, the *Wide* stretches).
  Clicking the active preset again in the inspector turns anchoring **off** and returns the
  control to `Position` mode.
- **`width` / `height`** — the *authored* size request.
- **`customMinSize` / `customMaxSize`** — hard clamps on the resolved size. A max of `0` on an
  axis means **unbounded**; the minimum always wins over the maximum.
- **`growDirectionH` / `growDirectionV`** — which edge stays put when the resolved size is
  pushed up to the minimum or down to the maximum.
- **Size flags** (`sizeFlagsHorizontal` / `sizeFlagsVertical` / `sizeFlagsStretchRatio`) — how
  the control behaves **inside a container**. Read by nothing else.

### What actually drives an axis

This is the thing to internalize, because editing the wrong property looks like the editor is
ignoring you. Exactly one of three things sizes each axis:

| Situation | The axis is driven by | `width`/`height` | offsets |
|---|---|---|---|
| `layoutMode = Position` | `width` / `height` | **read** | ignored |
| `Anchors`, and the two anchors on that axis are **equal** (point-anchored) | `width` / `height` | **read** | position only |
| `Anchors`, and the two anchors **differ** (stretched — every *Wide* and `FullRect` preset) | the **offsets** (extent = anchor span across the parent + offsets) | **ignored** | **read** |
| The parent is a **container** | the **container** | overwritten | overwritten |

`UIControl::GetAxisDriver(vertical)` returns exactly this, and the inspector uses it to grey
out the fields that cannot do anything in the current configuration (with a tooltip saying
why). The viewport gizmos use it too, so dragging a resize handle edits whichever property is
actually live — and refuses outright on a container-driven control rather than accepting the
drag and silently discarding it.

### Container-driven controls

When a control's parent is a container, the container calls `SetRect()` on it every layout
pass. That **overwrites the resolved rect and the node position** — anchors, offsets and
`width`/`height` are all ignored. What the child *can* say is its **size flags** and its
**`customMinSize`**; those are the container's inputs.

`SetRect()` deliberately does **not** write back into the `width` / `height` properties. Those
are the authored request and feed the next measure pass; writing the arranged size into them
would make a child ratchet larger every frame (it could grow but never shrink) and would
serialize transient layout output into the saved scene.

> Hit-testing and drawing use `GetBoundsSize()` (the anchored/resolved size), **not** raw
> `GetSize()`. Custom controls must follow this.

Clipping (`ClipsDescendants()` / `GetClipRect()`) is honored by **both** the renderer (a
scissor) and the pointer arbiter, so a control scrolled out of a `ScrollContainer` viewport is
neither drawn nor clickable.

UI nodes live in canvas space. A control's space follows `uiSpace` (World2D vs Canvas); see
`project_uicontrol_layout`. Use `UILayer` nodes for deterministic canvas-layer ordering, and
`Camera2D`/`CameraUI` for canvas camera control.

### Layout from script

The full layout surface is reachable through `ComponentRef:call` in Lua, mruby and Python:

```lua
local uc = self:get_node("%Panel"):get_component("Panel")

local r = uc:call("get_rect")          -- {x, y, width, height, left, right, top, bottom, ...}
local min = uc:call("get_min_size")    -- {x, y}
local want = uc:call("get_desired_size")

if uc:call("is_container_driven") then
    -- A container owns this rect: set size flags, not width/height.
    uc:call("set_size_flags", SizeFlags.Fill | SizeFlags.Expand, SizeFlags.Fill, 2.0)
else
    uc:call("set_anchor_preset", AnchorPreset.FullRect)
    uc:call("set_offsets", 16, 16, -16, -16)
end
```

`get_rect` returns both the raw min-corner form (`x`, `y`, `width`, `height`) and the named
canvas edges (`left`, `right`, `top`, `bottom`), so a script never has to know that `y` is the
bottom. The same surface exists in the C API (`lc_uicontrol_*`, see `05_c_api.md`).

## Pointer arbitration (no click pass-through)

UI input is polled: each control samples the mouse independently. To stop a single click from being handled by two overlapping controls, controls that consume the pointer (the **button family** — `Button`, `TextureButton`, `ToggleButton`, `TextureToggleButton`, `RadioButton`, `Checkbox`) gate their hit-test on a shared per-frame arbiter (`UIControl::IsTopPointerTarget`). Only the **front-most** consumer under the cursor handles the press; the topmost is the one with the highest render priority (`layer*1000 + sortingOrder`), ties broken by scene-tree draw order. The arbiter scans the tree once per frame per cursor position (cached on the input frame counter). Other controls can opt in by overriding `ConsumesPointerInput()` (return `true`) and `ContainsCanvasPoint()` (their hit area).

## Mouse filter (ColorRect, Panel)

`ColorRect` and `Panel` expose a **`mouseFilter`** enum property — the data-driven form of the same opt-in:

| Value | Behaviour |
|---|---|
| `Stop` (0) | The control handles the mouse **and** is opaque: it joins pointer arbitration, so buttons and other `Stop` controls behind it never see the click. |
| `Propagate` (1) | The control handles the mouse but is **not** opaque: controls behind it receive the same click too. |
| `Ignore` (2, **default**) | The control is invisible to the cursor: no mouse state, no signals, never blocks. This is the historical behaviour, so existing scenes are unaffected. |

A non-`Ignore` control emits **`mouse_entered`**, **`mouse_exited`**, **`mouse_pressed(button)`**, **`mouse_released(button)`** and **`clicked(button)`** (button: 0=left, 1=right, 2=middle). A release only fires on the control where the press began, and `clicked` only when the release also lands on it. State can be polled with `is_mouse_hovered()` / `is_mouse_pressed(button)`, and the filter read/written with `get_mouse_filter()` / `set_mouse_filter(filter)` — from Lua / mruby / Python and, in the C-API, via `lc_uicontrol_get_mouse_filter` / `lc_uicontrol_set_mouse_filter` / `lc_uicontrol_is_mouse_hovered` / `lc_uicontrol_is_mouse_pressed`.

The mechanism lives on `UIControl` (`DefineMouseFilterProperty()` / `DefineMouseFilterSignals()` / `UpdateMouseFilterState()`), so any other decorative control can adopt it the same way. The button family keeps its hard-coded opaque behaviour and ignores the filter.

## Keyboard / gamepad focus

A single control holds focus across the whole UI. Each control has a **`focusMode`** (`None` / `Click` / `All`, default `All`) plus per-side **`focusNeighbor*`** and **`focusNext` / `focusPrevious`** `NodePath` overrides. Directional traversal resolves the explicit neighbor when set, otherwise the nearest focusable control in that screen direction; next/previous fall back to scene-tree order. Decorative controls (labels, panels) should set `focusMode = None` so traversal skips them. The full method set (`grab_focus`, `find_valid_focus_neighbor`, `grab_focus_neighbor`, `find_next_valid_focus`, …) is scriptable from Lua / mruby / Python and the C-API — see [04_scripting.md](04_scripting.md#ui-focus--neighbor-navigation).

## Controls (catalog in `03`)

Buttons (`Button`, `ToggleButton`, `TextureButton`, `RadioButton`, `Checkbox`), text (`Label`, `RichTextLabel`, `LineEdit`, `TextEdit`), values (`Slider`, `SpinBox`, `ProgressBar`), lists (`Dropdown`, `ItemList`, `Tree`, `CheckList`, `RadioList`, `PopupMenu`), surfaces (`Panel`, `NineSlicePanel`, `ColorRect`, `StyleBox`, `UIImageDraw`).

Wire interactions via signals (see `14`):
```lua
function on_ready()
    Lupine.get_node("%StartButton"):connect("pressed", self, "on_start")
end
function on_start() Lupine.change_scene("res://scenes/game.scene") end
```

## Containers (auto-layout)

`HorizontalContainer`, `VerticalContainer`, `GridContainer`, `CenterContainer`,
`PaddingContainer`, `DockContainer`, `Stack`, `Wrap`, `ScrollContainer`, `TabContainer`,
`SplitContainer`, `AspectRatioContainer`, `Spacer`. `YSort` orders 2D draw by Y. Nest controls
under a container; the container drives their rects.

### Sizing model

A container measures each child with `GetDesiredSize()` (its authored `width`/`height` floored
to its content minimum and clamped to its custom min/max) and then arranges it with
`SetRect()`. Measure never reads the rect the container assigned last pass, so measuring is
idempotent and children cannot ratchet.

- **`padding`** (per side) — space *inside* the container, before its children.
- **`margin`** (per side, container children only) — space a *parent* container reserves
  *around* this container. It is added to the child's measurement and inset from its slot.
- **`separation`** — the gap between children, read by `HorizontalContainer`,
  `VerticalContainer` and `ScrollContainer`. The other containers have their own spacing
  properties (`horizontalSpacing`/`verticalSpacing` on Grid, `spacingX`/`spacingY` on Wrap,
  `dockSpacing` on Dock); the inspector greys `separation` out where it is not read.
- **`horizontalSizeMode` / `verticalSizeMode`** — `Fixed` (use `width`/`height`),
  `FitChildren`, `Expand`, `Minimum`. In any mode but `Fixed`, `width`/`height` are ignored.

### Size flags (the Godot model)

Per-child size flags are **authoritative on a container's cross axis**:

| Flag | Effect on the cross axis |
|---|---|
| `Fill` (default) | Take the container's whole cross-axis extent. |
| `ShrinkBegin` | Keep the desired extent, sit at the **start** — left on X, **TOP** on Y. |
| `ShrinkCenter` | Keep the desired extent, centered. |
| `ShrinkEnd` | Keep the desired extent, sit at the end — right on X, **BOTTOM** on Y. |
| `Expand` | (Main axis) claim a share of the free space, split by `sizeFlagsStretchRatio`. |

The flags are named in **reading order**, so `ShrinkBegin` on the vertical axis means the top
even though the canvas is Y-up. The container-wide alignment enum is only the fallback for
children that have **no `UIControl`** at all.

`Expand` follows Godot's model: the expanding children share a pool made of *their own desired
extents plus all the free space*, split by ratio. Two children with a desired 100 and ratios
1:3 in a 400px box end up **100 / 300** (not 150/250), and each is clamped to its own min/max.

### Main-axis distribution

`HorizontalContainer.horizontalAlignment` and `VerticalContainer.verticalAlignment` distribute
the free space along the stacking axis when nothing expands:

`Begin`, `Center`, `End`, `Fill`, plus the flexbox-style **`SpaceBetween`** (flush at both
ends, slack in the gaps), **`SpaceAround`** (equal space around each child, so the end gaps are
half) and **`SpaceEvenly`** (end gaps and inter-child gaps all equal).

### Per-child attached properties (`LayoutSlot`)

`DockContainer` and `Stack` need per-child settings that live on the *child*. Add a
**`LayoutSlot`** component to the child and set `dockSide` / `alignment` / `zIndex` /
`matchParent` / `ignoreLayout`. A child without one falls back to the container's default, so
adding the component is opt-in.

### SplitContainer / AspectRatioContainer

- **`SplitContainer`** — two panes divided by a draggable bar. `orientation`, `splitOffset`
  (pixels from the **start** edge — left, or **top**; `0` means centered), `splitterWidth`,
  `draggable`. Clamped so neither pane goes below its own minimum. Emits `dragged(offset)`.
- **`AspectRatioContainer`** — forces children to a `ratio` (width ÷ height) with a
  `stretchMode` of `Fit` (letterbox), `Cover` (overflow), `WidthControlsHeight` or
  `HeightControlsWidth`, aligned by `horizontalAlignment` / `verticalAlignment`.

## Text layout

`Label`, `Button`, `RichTextLabel`, `LineEdit`, `TextEdit` and the list/menu controls all lay
text out through the shared `TextLayout` (`core/include/lupine/rendering/TextLayout.hpp`), so
alignment, wrapping and measurement behave identically everywhere.

| Property | Meaning |
|---|---|
| `horizontalAlign` / `verticalAlign` (Label), `textHAlign` / `textVAlign` (Button) | Alignment inside the control's content box. `Fill` justifies by widening inter-word gaps (last line excluded). |
| `wordWrap` | The simple on/off switch. Equivalent to `autowrapMode = WordSmart`. |
| `autowrapMode` | `Off`, `Arbitrary` (break anywhere), `Word` (only at spaces — a too-long word overflows), `WordSmart` (at spaces, but split a word wider than the box). |
| `overrunBehavior` | `None`, `TrimChar`, `TrimWord`, `EllipsisChar`, `EllipsisWord`. Rewrites the *text* so it ends in "…" (or "..." when the font lacks U+2026). |
| `clipText` | Discards *glyphs* that fall outside the box. Orthogonal to `overrunBehavior`. |
| `lineSpacing` | Multiplier on the line advance. |
| `tabSize` | Tab stop width, in spaces (default 4). |
| `outlineWidth` / `outlineColor` | Stroke behind the glyphs. Counted in the content minimum size. |
| `shadowOffset` / `shadowColor` | Drop shadow. `+y` is **down the screen**, as an author expects. |
| `autoShrink` / `minFontSize` (Label) | Step the font down until the text fits the box. The complement of Button's `scaleMode = FitToText`, which grows the *control* to fit the *text*. |

Kerning is applied from the font's kern table (baked for the glyph range at load), so `AV` and
`To` are spaced correctly. Measurement and drawing use the same kern data, so a control never
measures at one width and then draws at another.

**Not supported: RTL / bidirectional text.** `TextLayout` is left-to-right only. Implementing
BiDi properly needs the Unicode bidirectional algorithm and script/shaping tables, which is a
separate body of work from this layout system. Scripts that require it (Arabic, Hebrew) will
lay out left-to-right and in logical rather than visual order.

### One content-inset model

There used to be three incompatible padding schemes and a fourth that was dead. They are now
one: **`UIControl::GetContentMargins()`** returns the per-side inset applied to a control's
text, as the component-wise **maximum** of

1. the control's own padding property — `textPadding` (Vec2) on Button, the scalar `padding`
   on `LineEdit`/`TextEdit`/`Dropdown`/…, the Vec4 `padding` on a Container — and
2. the **content margins of its themed `StyleBox`** (`contentMarginLeft` and friends).

So authoring either one works, and neither silently zeroes the other. StyleBox content margins
now actually move the text; before, they were serialized, cloned, unit-tested and editable in
the theme editor while having **zero** consumers.

## Custom shaders on 2D UI

`ColorRect`, `Image2D`, `Panel`, `Shape2D` accept custom `.lsh` shaders via `CustomShaderParams` (typed JSON params, live mtime reload). The `.lsh` language — directives, built-ins (`TIME`, `FRONT_FACING`, `TEXTURE_PIXEL_SIZE`, …), `#render_mode`, sampler hints, and `u_SceneTexture` grab-pass — is documented in `19_shaders.md`. The same `.lsh` shaders also attach to 3D meshes.

## Theme system

Theme assets: `.uitheme` (styles) + `.palette` (color tokens), `lupine::ui::Theme`/`ThemeManager`. Design tokens (colors, constants, vec2s, bools, fonts, themed images, styleboxes) resolve per control type + entry.

Script access:
```lua
Lupine.set_theme("res://ui/main.uitheme")
local c = Lupine.get_theme_color("Button", "font_color")
local k = Lupine.get_theme_constant("Panel", "corner_radius")
Lupine.set_palette_color("accent", 0.2, 0.6, 1.0, 1.0)
Lupine.set_theme_variable("key", value)
```
Project defaults: `misc.default_font`, `misc.default_theme` (res:// paths). Themeable image entries support background images on `Button`/`Panel` and textures on `TextureButton`. (Note: `editor/theme.py` styles the editor chrome, not game UI.)

### Theme-level defaults (Godot Theme parity)

A `.uitheme` may set three top-level fields, inherited down the `extends` chain:
- `default_font` (res:// path) — fallback font for any text control whose type+entry resolves no font.
- `default_font_size` (number) — fallback size when no `font_size` constant / font role applies.
- `default_base_scale` (number, default 1.0) — multiplies every resolved font size and themed constant.

### StyleBox types (Godot parity)

Theme stylebox entries and the `StyleBox` data model support four concrete subtypes (a stylebox entry's embedded `"stylebox"` object carries a `"type"` field):
- `StyleBoxFlat` — background + per-side border + per-corner radius + shadow + expand margins.
- `StyleBoxTexture` — a 9-patch textured box: `texturePath`, optional `regionRect`, per-side `textureMargin*` (source pixels), `expandMargin*`, `modulateColor`, `drawCenter`, and per-axis `axisStretch*` (`stretch`/`tile`/`tile_fit`).
- `StyleBoxLine` — a separator line: `color`, `thickness`, `growBegin`/`growEnd`, `vertical`.
- `StyleBoxEmpty` — draws nothing (content margins only).

`components::DrawStyleBox(ctx, box, center, size, rotation, modulate)` (in `StyleBoxRenderer.hpp`) is the single painter for all four (`modulate` tints every painted colour — opacity or interaction-state modulation). Controls render a themed stylebox in place of their flat background when the effective theme defines the matching entry:
- `Panel` → `"panel"`.
- `Button` → per-state `"normal"`/`"hover"`/`"pressed"`/`"disabled"` (Godot Button vocabulary).
- `ToggleButton` → same vocabulary, with toggled-on → `"pressed"` and toggled+hover → `"hover_pressed"`.

The state modulation colour tints the resolved stylebox, so hover/pressed brightness still applies. When the theme defines no entry for the current state, the control falls back to its flat property-driven style (zero regression).

### Type variations (variants)

A *variant* is a theme type whose `extends` names the base type it varies (e.g. type `PrimaryButton` with `extends: Button`). Set a control's `themeTypeVariation` property to the variant name; resolution checks the variation first, then the base type. Declare and edit variants in the UI Theme editor; pick a control's variant from the inspector.

### Nine-slice on image/texture controls

Every control that paints a texture/image background supports configurable 9-patch slicing — `Button`/`Panel` (background image), `Image2D`, `TextureButton`, `TextureToggleButton` — via per-side margins (source texture pixels), per-axis stretch/tile, and an optional center fill. `NineSlicePanel` remains the dedicated 9-patch surface. All share the `DrawUIImage` painter.

A **theme image entry** may also dictate the fit, not just the path: it carries an optional `stretch_mode` (`stretch`/`keep_centered`/`nine_slice`) and, for nine-slice, a `nine_slice` object (`left`/`top`/`right`/`bottom` margins, `axis_h`/`axis_v` `stretch`|`tile`, `draw_center`). When set, it overrides the control's own nine-slice properties — so a theme can set its `TextureButton` state images (and `Button`/`Panel` background images) to nine-slice and supply the margins. Authored per image entry in the UI Theme editor's image rows (Fit → Nine-Slice).
