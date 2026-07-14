# 19 — Lupine Shaders (`.lsh`)

Lupine has a single unified shader language, `.lsh`, that is translated at runtime to
the active graphics backend (OpenGL, Vulkan, WebGL, DirectX 11/12, Metal). The same
shader source compiles everywhere; you write GLSL-like code with a few cross-platform
macros and directives.

Custom `.lsh` shaders can be attached to:

- **2D UI / sprites** — `ColorRect`, `Image2D`, `Panel`, `Shape2D` (via `materialOverride`
  + `shaderParameters`; see `12_ui_and_theme.md`).
- **3D meshes** — `StaticMesh3D` and `SkeletalMesh3D` (per material slot, "LSH" field in the
  Material Slots inspector), `PrimitiveMesh3D` (`customLshShaderPath` property), and
  `MultiMeshGeneric` (`customLshShaderPath`; the shader must be *instanced* — see below). A
  `.lsh` shader takes precedence over the slot/component shader type when set.

## File structure

```glsl
#shader "MyShader"
#description "What it does"
#feature lighting          // optional: lighting | shadows | normal_mapping | fog
#render_mode cull_disabled, blend_mix, depth_draw_opaque   // optional, see below

#begin properties
    uniform mat4 u_ViewProjection;
    uniform mat4 u_Model;
    uniform vec4 u_AlbedoColor = vec4(1.0);
    uniform sampler2D u_Texture; // @source_color "true" @filter "linear"
#end properties

#begin shared
    // helper functions shared by stages (plain GLSL math; no SAMPLE/MUL macros here)
#end shared

#begin vertex
    #input vec3 a_Position : POSITION 0
    #input vec2 a_TexCoord : TEXCOORD0 1
    #output vec2 v_TexCoord
    void main() {
        v_TexCoord = a_TexCoord;
        VERTEX_OUTPUT = MUL(u_ViewProjection, MUL(u_Model, vec4(a_Position, 1.0)));
    }
#end vertex

#begin fragment
    #output vec4 FragColor
    void main() {
        FragColor = SAMPLE(u_Texture, v_TexCoord) * u_AlbedoColor;
    }
#end fragment
```

Standard engine-bound uniforms (filled automatically, never shown as editable params):
`u_ViewProjection`, `u_Model`, `u_View`, `u_NormalMatrix`, `u_CameraPosition`, `u_TintColor`,
`u_AlbedoColor`, `u_Texture`/`u_AlbedoTexture`, `u_Time`, `u_TexelSize`, `u_Resolution`,
`u_SceneTexture`, plus the UI set (`u_CornerRadius`, `u_Size`, `u_UVRect`, …). Any other
declared uniform is an exported parameter the user can edit.

## Cross-platform macros

| Macro | Meaning |
|-------|---------|
| `VERTEX_OUTPUT` | Clip-space position output (`gl_Position` / `SV_POSITION` / `[[position]]`) |
| `SAMPLE(t, uv)` / `SAMPLE_LOD(t, uv, lod)` / `SAMPLE_CUBE(t, dir)` | Texture sampling |
| `MUL(a, b)` | Matrix/vector multiply (operator on GLSL/Metal, `mul()` on HLSL) |
| `MAT3(m)` | `mat4` → `mat3` cast |
| `DISCARD;` | Fragment discard |
| `FRAG_COORD`, `INSTANCE_ID`, `VERTEX_ID` | Built-in fragment/vertex indices |
| `TIME` | Per-frame elapsed seconds (fed each frame by the renderer) |
| `FRONT_FACING` | `bool`, true for front-facing fragments (back-face card flips, two-sided) |
| `TEXTURE_SIZE(tex)` | `vec2` texel dimensions of a sampler |
| `TEXTURE_PIXEL_SIZE(tex)` | `1.0 / TEXTURE_SIZE(tex)` (Godot's `TEXTURE_PIXEL_SIZE`) |
| `PRIMARY_SHADOW_ATTENUATION(worldPos, normal)` | Directional-light shadow factor (1=lit, 0=shadowed); requires `#feature shadows` |

## `#render_mode` (pipeline state)

Declares blend/cull/depth state, mirroring Godot spatial `render_mode` token names.
Whitespace- or comma-separated. When omitted, the host decides defaults (2D UI: blend-per
component, no depth, no cull; 3D mesh: opaque, depth test + write, back-face cull).

| Token | Effect |
|-------|--------|
| `cull_back` / `cull_front` / `cull_disabled` | Face culling (`cull_disabled` = two-sided) |
| `blend_mix` / `blend_add` / `blend_sub` / `blend_mul` / `blend_premul` / `blend_opaque` | Blend mode |
| `depth_test_disabled` | Disable depth testing |
| `depth_draw_never` / `depth_draw_opaque` / `depth_draw_always` | Depth writing |
| `unshaded` | Hint: shader does its own (or no) lighting |

Example two-sided, additive card: `#render_mode cull_disabled, blend_add`.

## Sampler hints (Gap G)

Per-sampler annotations in a trailing comment on the uniform line:

| Annotation | Effect |
|-----------|--------|
| `@source_color "true"` | Decode sRGB→linear when sampling (textures load as UNORM, so the decode is applied in-shader) |
| `@filter "nearest"` / `"linear"` | Sampler filter mode |
| `@repeat "repeat"` / `"clamp"` / `"mirror"` | Sampler address mode |
| `@hint_default "black"` / `"white"` / `"normal"` | Fallback when the texture is unbound |

`@display "Label"`, `@range "min,max"`, `@color "true"` control how a parameter appears in
the inspector.

## Lighting & shadows (spatial shaders)

Declare `#feature lighting` (and `shadows`, `fog`, `normal_mapping` as needed). The engine
then exposes the forward-lighting infrastructure to your fragment shader:

- `u_Lights` — the light UBO (`u_Lights.lights[i]` with `.positionOrDirection`, `.direction`,
  `.color`, `.params`, `.flags`; `.lightCounts.x` = active count; `.ambientLight`).
- `calculateShadow(smIndex, worldPos, N, L, lightPos)` — PCF shadow factor for a light.
- `applyFog(color, worldPos, cameraPos)` — distance fog.
- `PRIMARY_SHADOW_ATTENUATION(worldPos, N)` — convenience: shadow factor of the first
  shadow-casting directional light (the "unshaded × directional-shadow" look).

Write your own per-light loop (as the built-in `pbr`/`toon` shaders do) to set the final
color — there is no implicit `ALBEDO`/`light()` model; you are in full control.

```glsl
// Unshaded surface that still receives the sun's shadow:
#feature lighting
#feature shadows
#render_mode unshaded
// fragment:
float s = PRIMARY_SHADOW_ATTENUATION(v_WorldPos, normalize(v_Normal));
FragColor = vec4(u_AlbedoColor.rgb * mix(0.4, 1.0, s), 1.0);
```

## Mid-scene screen reads

A shader that declares `uniform sampler2D u_SceneTexture;` becomes a grab-pass: the engine
renders the scene-so-far (excluding grab objects) into an offscreen target and binds it as
`u_SceneTexture` before drawing the object, so it can sample what is behind it (refraction,
distortion, blur). Works on both 2D UI and 3D mesh shaders. Compute the screen UV in the
vertex stage from the clip position. When two or more grab objects overlap, the renderer
falls into an *ordered* capture so each later grabber also refracts the earlier ones (drawn
one at a time, each sampling a snapshot of everything before it). Full-screen color grading
is better done with `CameraEffect` components (see `03_nodes_and_components.md`).

## Instanced shaders (MultiMesh)

A `.lsh` attached to `MultiMeshGeneric` must be instanced: declare the per-instance vertex
inputs the engine supplies from the instance buffer (binding 1) and reconstruct the world
transform from its four columns (backend-agnostic — avoid `mat4(...)` whose column order
differs across backends), exactly like the built-in `pbr_instanced.lsh`:

```glsl
#input vec4 a_InstanceModel0 : TEXCOORD1 4
#input vec4 a_InstanceModel1 : TEXCOORD2 5
#input vec4 a_InstanceModel2 : TEXCOORD3 6
#input vec4 a_InstanceModel3 : TEXCOORD4 7
#input vec4 a_InstanceColor  : TEXCOORD5 8
#input vec4 a_InstanceCustom : TEXCOORD6 9
// vertex:
vec4 worldPos = a_InstanceModel0 * a_Position.x + a_InstanceModel1 * a_Position.y
              + a_InstanceModel2 * a_Position.z + a_InstanceModel3;
```

## Authoring notes

- Two transpilers must stay in lock-step: the build-time Python transpiler
  (`core/shaders/lupine_shader_transpiler.py`, regenerates `GeneratedShaders.hpp` for the
  ~28 built-ins) and the runtime C++ translator (`core/src/rendering/ShaderTranslator.cpp`,
  used for custom user shaders). Any change to one is mirrored in the other.
- Validate generated output with `glslangValidator` (GLSL/Vulkan) and `dxc` (HLSL).
- `mod(x, y)` is floored on all backends (HLSL/Metal `fmod` truncates — the transpiler
  expands `mod` to the floored form there).
