#!/usr/bin/env python3
"""
Lupine Shader Transpiler
========================
Parses .lsh (Lupine Shader) unified shader files and generates backend-specific shader code
for all supported graphics backends: OpenGL, Vulkan, WebGL, DirectX11, DirectX12, Metal.

The Lupine Shader Language uses GLSL-like syntax with cross-platform macros:
  VERTEX_OUTPUT    - Clip-space position output (gl_Position / SV_POSITION / [[position]])
  SAMPLE(t, uv)    - Texture sampling
  SAMPLE_LOD(t, uv, lod) - Texture sampling with explicit LOD
  SAMPLE_CUBE(t, d) - Cubemap sampling
  MUL(a, b)         - Matrix/vector multiplication
  MAT3(m)            - mat4 to mat3 cast
  DISCARD            - Fragment discard
  INSTANCE_ID        - Instance index
  VERTEX_ID          - Vertex index
  FRAG_COORD         - Fragment coordinate
"""

import re
import os
import sys
from dataclasses import dataclass, field
from enum import Enum
from typing import Optional


class Backend(Enum):
    OPENGL = "OpenGL"
    VULKAN = "Vulkan"
    WEBGL = "WebGL"
    DIRECTX11 = "DirectX11"
    DIRECTX12 = "DirectX12"
    METAL = "Metal"


@dataclass
class UniformInfo:
    type_name: str       # GLSL type: vec4, mat4, sampler2D, etc.
    name: str
    default_value: str = ""
    category: str = ""
    display_name: str = ""
    is_texture: bool = False
    is_cubemap: bool = False
    array_size: int = 0  # 0 = not an array
    # Sampler hints (Gap G) — only meaningful for texture uniforms.
    source_color: bool = False   # decode sRGB->linear on sample
    filter: str = ""             # "nearest" | "linear" (engine sampler state)
    repeat: str = ""             # "repeat" | "clamp" | "mirror" (engine sampler state)
    hint_default: str = ""       # "black" | "white" | "normal" (fallback when unbound)


@dataclass
class InputInfo:
    type_name: str
    name: str
    semantic: str        # POSITION, NORMAL, TEXCOORD0, COLOR, etc.
    location: int


@dataclass
class OutputInfo:
    type_name: str
    name: str
    semantic: str = ""   # For fragment: COLOR0, etc.


@dataclass
class StageCode:
    inputs: list = field(default_factory=list)
    outputs: list = field(default_factory=list)
    body: str = ""
    functions: str = ""
    topology: str = ""        # For geometry: "point", "line", "triangle"
    max_vertices: int = 0     # For geometry: max output vertices


VALID_FEATURES = {'lighting', 'shadows', 'normal_mapping', 'fog'}


@dataclass
class RenderMode:
    """Pipeline render state declared via `#render_mode` (Gap B). Defaults match a
    Godot spatial material: opaque blend, back-face culling, depth test + write.
    `specified` is False until a `#render_mode` directive sets at least one token,
    letting the renderer keep host-driven defaults (2D-UI blend, etc.) when absent."""
    specified: bool = False
    cull: str = "back"          # back | front | disabled
    blend: str = "mix"          # mix | add | sub | mul | premul | opaque
    depth_test: bool = True
    depth_write: bool = True
    unshaded: bool = False


@dataclass
class ShaderIR:
    name: str = ""
    description: str = ""
    features: set = field(default_factory=set)  # e.g. {"lighting", "shadows", "normal_mapping"}
    properties: list = field(default_factory=list)
    shared_code: str = ""
    vertex: Optional[StageCode] = None
    fragment: Optional[StageCode] = None
    geometry: Optional[StageCode] = None
    render_mode: 'RenderMode' = field(default_factory=lambda: RenderMode())

    # Built-in usage flags computed after parsing (drive entry-signature plumbing).
    uses_time: bool = False
    uses_front_facing: bool = False
    uses_primary_shadow: bool = False

    @property
    def has_lighting(self) -> bool:
        return 'lighting' in self.features or 'shadows' in self.features or 'fog' in self.features

    @property
    def has_shadows(self) -> bool:
        return 'shadows' in self.features

    @property
    def has_normal_mapping(self) -> bool:
        return 'normal_mapping' in self.features

    @property
    def has_fog(self) -> bool:
        return 'fog' in self.features


# ==============================================================================
# Parser
# ==============================================================================

class LupineShaderParser:
    """Parses .lsh shader files into an intermediate representation."""

    def parse(self, source: str) -> ShaderIR:
        ir = ShaderIR()
        lines = source.split('\n')
        i = 0
        while i < len(lines):
            line = lines[i].strip()

            if line.startswith('#shader'):
                m = re.match(r'#shader\s+"([^"]*)"', line)
                if m:
                    ir.name = m.group(1)

            elif line.startswith('#description'):
                m = re.match(r'#description\s+"([^"]*)"', line)
                if m:
                    ir.description = m.group(1)

            elif line.startswith('#feature'):
                m = re.match(r'#feature\s+(\w+)', line)
                if m and m.group(1) in VALID_FEATURES:
                    ir.features.add(m.group(1))
                    # shadows and fog imply lighting (fog data lives in the light UBO)
                    if m.group(1) in ('shadows', 'fog'):
                        ir.features.add('lighting')

            elif line.startswith('#render_mode'):
                self._parse_render_mode(line, ir)

            elif line == '#begin properties':
                i, props = self._parse_properties(lines, i + 1)
                ir.properties = props
                continue

            elif line == '#begin shared':
                i, code = self._parse_block(lines, i + 1, 'shared')
                ir.shared_code = code
                continue

            elif line == '#begin vertex':
                i, stage = self._parse_stage(lines, i + 1, 'vertex')
                ir.vertex = stage
                continue

            elif line == '#begin fragment':
                i, stage = self._parse_stage(lines, i + 1, 'fragment')
                ir.fragment = stage
                continue

            elif line == '#begin geometry':
                i, stage = self._parse_stage(lines, i + 1, 'geometry')
                ir.geometry = stage
                continue

            i += 1
        self._post_process(ir)
        return ir

    def _parse_render_mode(self, line: str, ir: ShaderIR):
        """Parse `#render_mode <tokens>` (Gap B) into pipeline state. Tokens mirror Godot
        spatial render_mode names. Whitespace- or comma-separated."""
        rm = ir.render_mode
        rm.specified = True
        body = re.sub(r'//.*$', '', line[len('#render_mode'):]).strip()
        for tok in re.split(r'[\s,]+', body):
            t = tok.strip().lower()
            if not t:
                continue
            if t in ('cull_back', 'cull_front', 'cull_disabled'):
                rm.cull = t[len('cull_'):]
            elif t in ('blend_mix', 'blend_add', 'blend_sub', 'blend_mul',
                       'blend_premul', 'blend_opaque'):
                rm.blend = t[len('blend_'):]
            elif t == 'opaque':
                rm.blend = 'opaque'
            elif t == 'depth_test_disabled':
                rm.depth_test = False
            elif t == 'depth_draw_never':
                rm.depth_write = False
            elif t in ('depth_draw_always', 'depth_draw_opaque', 'depth_prepass_alpha'):
                rm.depth_write = True
            elif t == 'unshaded':
                rm.unshaded = True

    def _post_process(self, ir: ShaderIR):
        """Detect built-in usage across stage bodies and inject system uniforms.

        `TIME` (Gap E) becomes the engine-fed `u_Time` float; it is injected as a
        regular property so every emitter declares and routes it like any uniform
        (the renderer fills it each frame). `FRONT_FACING` (Gap D) only flags the
        fragment entry so HLSL/Metal can add the system input."""
        vbody = ir.vertex.body if ir.vertex else ""
        fbody = ir.fragment.body if ir.fragment else ""
        both = vbody + "\n" + fbody

        ir.uses_time = bool(re.search(r'\bTIME\b', both))
        ir.uses_front_facing = bool(re.search(r'\bFRONT_FACING\b', fbody))
        ir.uses_primary_shadow = bool(re.search(r'\bPRIMARY_SHADOW_ATTENUATION\b', fbody))

        if ir.uses_time and not any(u.name == 'u_Time' for u in ir.properties):
            ir.properties.append(UniformInfo(
                type_name='float', name='u_Time', default_value='0.0'))

    def _parse_properties(self, lines, start):
        props = []
        i = start
        while i < len(lines):
            line = lines[i].strip()
            if line == '#end properties':
                return i + 1, props
            if line and not line.startswith('//'):
                u = self._parse_uniform(line)
                if u:
                    props.append(u)
            i += 1
        return i, props

    def _parse_uniform(self, line: str) -> Optional[UniformInfo]:
        # Remove inline comments but preserve annotation comments
        annotations = {}
        ann_matches = re.findall(r'@(\w+)\s+"([^"]*)"', line)
        for key, val in ann_matches:
            annotations[key] = val
        line_clean = re.sub(r'//.*$', '', line).strip()
        line_clean = re.sub(r'@\w+\s+"[^"]*"', '', line_clean).strip()
        if line_clean.endswith(';'):
            line_clean = line_clean[:-1].strip()

        # Match: uniform TYPE NAME = DEFAULT
        # or:    uniform TYPE NAME[SIZE] = DEFAULT
        # or:    uniform TYPE NAME
        m = re.match(r'uniform\s+(\w+)\s+(\w+)(?:\[(\d+)\])?\s*(?:=\s*(.+))?', line_clean)
        if not m:
            return None

        type_name = m.group(1)
        name = m.group(2)
        array_size = int(m.group(3)) if m.group(3) else 0
        default = m.group(4).strip() if m.group(4) else ""

        # Sampler hints (Gap G). @source_color may be a bare flag or @source_color "true".
        source_color = (annotations.get('source_color', '').lower() in ('true', '1', 'yes')
                        or bool(re.search(r'@source_color\b(?!\s*")', line)))

        return UniformInfo(
            type_name=type_name,
            name=name,
            default_value=default,
            category=annotations.get('category', ''),
            display_name=annotations.get('display', ''),
            is_texture=(type_name in ('sampler2D', 'sampler2DShadow')),
            is_cubemap=(type_name == 'samplerCube'),
            array_size=array_size,
            source_color=source_color,
            filter=annotations.get('filter', ''),
            repeat=annotations.get('repeat', ''),
            hint_default=annotations.get('hint_default', ''),
        )

    def _parse_stage(self, lines, start, stage_name):
        stage = StageCode()
        body_lines = []
        i = start
        in_body = False

        while i < len(lines):
            line = lines[i]
            stripped = line.strip()

            if stripped == f'#end {stage_name}':
                stage.body = '\n'.join(body_lines)
                return i + 1, stage

            if stripped.startswith('#input'):
                inp = self._parse_input(stripped)
                if inp:
                    stage.inputs.append(inp)
            elif stripped.startswith('#output'):
                out = self._parse_output(stripped)
                if out:
                    stage.outputs.append(out)
            elif stripped.startswith('#topology'):
                m = re.match(r'#topology\s+(\w+)', stripped)
                if m:
                    stage.topology = m.group(1)
            elif stripped.startswith('#maxvertices'):
                m = re.match(r'#maxvertices\s+(\d+)', stripped)
                if m:
                    stage.max_vertices = int(m.group(1))
            else:
                body_lines.append(line)

            i += 1

        stage.body = '\n'.join(body_lines)
        return i, stage

    def _parse_input(self, line: str) -> Optional[InputInfo]:
        # #input vec3 a_Position : POSITION 0
        m = re.match(r'#input\s+(\w+)\s+(\w+)\s*:\s*(\w+)\s+(\d+)', line)
        if m:
            return InputInfo(m.group(1), m.group(2), m.group(3), int(m.group(4)))
        return None

    def _parse_output(self, line: str) -> Optional[OutputInfo]:
        # #output vec4 FragColor
        # or: #output vec4 FragColor : COLOR0
        m = re.match(r'#output\s+(\w+)\s+(\w+)(?:\s*:\s*(\w+))?', line)
        if m:
            return OutputInfo(m.group(1), m.group(2), m.group(3) or "")
        return None

    def _parse_block(self, lines, start, block_name):
        body_lines = []
        i = start
        while i < len(lines):
            stripped = lines[i].strip()
            if stripped == f'#end {block_name}':
                return i + 1, '\n'.join(body_lines)
            body_lines.append(lines[i])
            i += 1
        return i, '\n'.join(body_lines)


# ==============================================================================
# Type / Function Mappings
# ==============================================================================

GLSL_TO_HLSL_TYPES = {
    'vec2': 'float2', 'vec3': 'float3', 'vec4': 'float4',
    'ivec2': 'int2', 'ivec3': 'int3', 'ivec4': 'int4',
    'uvec2': 'uint2', 'uvec3': 'uint3', 'uvec4': 'uint4',
    'bvec2': 'bool2', 'bvec3': 'bool3', 'bvec4': 'bool4',
    'mat2': 'float2x2', 'mat3': 'float3x3', 'mat4': 'float4x4',
    'sampler2D': 'Texture2D', 'samplerCube': 'TextureCube',
    'sampler2DShadow': 'Texture2D',
}

GLSL_TO_METAL_TYPES = {
    'vec2': 'float2', 'vec3': 'float3', 'vec4': 'float4',
    'ivec2': 'int2', 'ivec3': 'int3', 'ivec4': 'int4',
    'uvec2': 'uint2', 'uvec3': 'uint3', 'uvec4': 'uint4',
    'bvec2': 'bool2', 'bvec3': 'bool3', 'bvec4': 'bool4',
    'mat2': 'float2x2', 'mat3': 'float3x3', 'mat4': 'float4x4',
}

# NOTE: GLSL 'mod' is intentionally NOT mapped to 'fmod'. GLSL mod is floored
# (result takes the sign of the divisor) while HLSL/Metal fmod is truncated (sign
# of the dividend); they differ for negative operands. mod is expanded separately
# to the floored equivalent (x - y * floor(x / y)) — see _expand_mod_to_floored.
HLSL_FUNC_REPLACEMENTS = {
    'mix': 'lerp',
    'fract': 'frac',
    'dFdx': 'ddx',
    'dFdy': 'ddy',
    'inversesqrt': 'rsqrt',
    'atan': 'atan2',
}

METAL_FUNC_REPLACEMENTS = {
    'dFdx': 'dfdx',
    'dFdy': 'dfdy',
    'inversesqrt': 'rsqrt',
}

HLSL_SEMANTIC_MAP = {
    'POSITION': 'POSITION',
    'NORMAL': 'NORMAL',
    'TEXCOORD0': 'TEXCOORD0',
    'TEXCOORD1': 'TEXCOORD1',
    'TEXCOORD2': 'TEXCOORD2',
    'TEXCOORD3': 'TEXCOORD3',
    'COLOR': 'COLOR',
    'TANGENT': 'TANGENT',
    'BLENDWEIGHT': 'BLENDWEIGHT',
    'BLENDINDICES': 'BLENDINDICES',
}


def hlsl_type(glsl_type: str) -> str:
    return GLSL_TO_HLSL_TYPES.get(glsl_type, glsl_type)


_HLSL_CONST_TYPE_RE = re.compile(
    r'const\s+(?:float|float2|float3|float4|float2x2|float3x3|float4x4|'
    r'int|int2|int3|int4|uint|uint2|uint3|uint4|half|double|bool)\b')


def hlsl_static_const(code: str) -> str:
    """Prefix global-scope 'const TYPE' declarations with 'static' for HLSL.

    In HLSL a global 'const' is an external constant that silently reads as
    zero; compile-time literals need 'static const'.  Only tokens at
    brace/paren depth zero are rewritten so function locals and parameters
    keep plain 'const', and tokens already preceded by 'static' are skipped
    (a blind regex would emit 'static static const').
    """
    out = []
    brace = 0
    paren = 0
    i = 0
    n = len(code)

    def is_word(ch: str) -> bool:
        return ch.isalnum() or ch == '_'

    while i < n:
        c = code[i]
        if c == '{':
            brace += 1
        elif c == '}':
            brace -= 1
        elif c == '(':
            paren += 1
        elif c == ')':
            paren -= 1
        elif (c == 'c' and brace == 0 and paren == 0
              and (i == 0 or not is_word(code[i - 1]))
              and _HLSL_CONST_TYPE_RE.match(code, i)):
            j = i
            while j > 0 and code[j - 1] in ' \t':
                j -= 1
            preceded_by_static = (j >= 6 and code[j - 6:j] == 'static'
                                  and (j == 6 or not is_word(code[j - 7])))
            if not preceded_by_static:
                out.append('static ')
        out.append(c)
        i += 1
    return ''.join(out)


def metal_type(glsl_type: str) -> str:
    return GLSL_TO_METAL_TYPES.get(glsl_type, glsl_type)


def get_type_size(type_name: str) -> int:
    """Returns byte size for HLSL cbuffer alignment."""
    sizes = {
        'float': 4, 'int': 4, 'uint': 4, 'bool': 4,
        'vec2': 8, 'float2': 8, 'ivec2': 8, 'int2': 8,
        'vec3': 12, 'float3': 12, 'ivec3': 12,
        'vec4': 16, 'float4': 16, 'ivec4': 16,
        'mat3': 48, 'float3x3': 48,  # 3 * 16 (padded rows)
        'mat4': 64, 'float4x4': 64,
    }
    return sizes.get(type_name, 16)


def get_type_alignment(type_name: str) -> int:
    """Returns alignment for HLSL cbuffer packing (std140-like)."""
    if type_name in ('mat4', 'mat3', 'float4x4', 'float3x3'):
        return 16
    if type_name in ('vec4', 'float4', 'ivec4', 'int4', 'uvec4', 'uint4'):
        return 16
    if type_name in ('vec3', 'float3', 'ivec3', 'int3'):
        return 16  # vec3 aligns to 16 in std140/HLSL
    if type_name in ('vec2', 'float2', 'ivec2', 'int2'):
        return 8
    return 4


# ==============================================================================
# Lighting Feature Code Generation
# ==============================================================================

# Shared struct definitions in GLSL syntax (transformed per backend)
_LIGHT_STRUCTS_GLSL = """\
struct Light {
    vec4 positionOrDirection;
    vec4 direction;
    vec4 color;
    vec4 params;
    vec4 flags;
};
struct ShadowMap {
    mat4 lightSpaceMatrix;
    vec4 shadowParams;
    vec4 shadowParams2;
};
struct CascadedShadowMap {
    mat4 cascadeMatrices[8];
    vec4 cascadeSplits;
    vec4 cascadeSplits2;
    vec4 cascadeParams;
    vec4 cascadeParams2;
};"""

_LIGHT_STRUCTS_HLSL = """\
struct Light {
    float4 positionOrDirection;
    float4 direction;
    float4 color;
    float4 params;
    float4 flags;
};
struct ShadowMap {
    float4x4 lightSpaceMatrix;
    float4 shadowParams;
    float4 shadowParams2;
};
struct CascadedShadowMap {
    float4x4 cascadeMatrices[8];
    float4 cascadeSplits;
    float4 cascadeSplits2;
    float4 cascadeParams;
    float4 cascadeParams2;
};"""

_LIGHT_STRUCTS_METAL = """\
constant int MAX_LIGHTS = 16;
constant int MAX_SHADOW_MAPS = 8;
constant int MAX_CASCADES = 8;

struct Light {
    float4 positionOrDirection;
    float4 direction;
    float4 color;
    float4 params;
    float4 flags;
};
struct ShadowMapData {
    float4x4 lightSpaceMatrix;
    float4 shadowParams;
    float4 shadowParams2;
};
struct CascadedShadowMapData {
    float4x4 cascadeMatrices[MAX_CASCADES];
    float4 cascadeSplits;
    float4 cascadeSplits2;
    float4 cascadeParams;
    float4 cascadeParams2;
};
struct LightUniformBuffer {
    Light lights[MAX_LIGHTS];
    ShadowMapData shadowMaps[MAX_SHADOW_MAPS];
    CascadedShadowMapData cascadedShadowMaps[MAX_SHADOW_MAPS];
    float4 ambientLight;
    float4 lightCounts;
    float4 fogColor;
    float4 fogParams;
};"""

# UBO fields (used by GLSL/HLSL declarations, not Metal which uses struct above)
_LIGHT_UBO_FIELDS_GLSL = """\
    Light lights[16];
    ShadowMap shadowMaps[8];
    CascadedShadowMap cascadedShadowMaps[8];
    vec4 ambientLight;
    vec4 lightCounts;
    vec4 fogColor;
    vec4 fogParams;"""

_LIGHT_UBO_FIELDS_HLSL = """\
    Light lights[16];
    ShadowMap shadowMaps[8];
    CascadedShadowMap cascadedShadowMaps[8];
    float4 ambientLight;
    float4 lightCounts;
    float4 fogColor;
    float4 fogParams;"""


def _gen_shadow_sampler_gl(num_2d: int, num_cube: int) -> str:
    """Generate if/else shadow sampling functions for OpenGL/WebGL."""
    lines = []
    # 2D shadow maps
    for i in range(num_2d):
        lines.append(f'uniform sampler2D u_ShadowMap{i};')
    for i in range(num_cube):
        lines.append(f'uniform samplerCube u_ShadowCubeMap{i};')
    lines.append('')
    # sampleShadowMap
    lines.append('float sampleShadowMap(int index, vec2 uv) {')
    for i in range(num_2d):
        prefix = '    if' if i == 0 else '    else if'
        lines.append(f'{prefix} (index == {i}) return texture(u_ShadowMap{i}, uv).r;')
    lines.append('    return 1.0;')
    lines.append('}')
    lines.append('')
    # sampleShadowCubeMap
    lines.append('float sampleShadowCubeMap(int index, vec3 dir) {')
    lines.append('    vec4 s;')
    if num_cube > 0:
        for i in range(num_cube):
            prefix = '    if' if i == 0 else '    else if'
            lines.append(f'{prefix} (index == {i}) s = texture(u_ShadowCubeMap{i}, dir);')
        lines.append('    else return 1.0;')
    else:
        lines.append('    return 1.0;')
    lines.append('    return s.r;')
    lines.append('}')
    return '\n'.join(lines)


def _gen_shadow_sampler_vulkan() -> str:
    """Generate array-based shadow sampling for Vulkan."""
    return """\
layout(set = 0, binding = 8) uniform sampler2D u_ShadowMaps[8];
layout(set = 0, binding = 9) uniform samplerCube u_ShadowCubeMaps[8];

float sampleShadowMap(int index, vec2 uv) {
    return texture(u_ShadowMaps[index], uv).r;
}
float sampleShadowCubeMap(int index, vec3 dir) {
    return texture(u_ShadowCubeMaps[index], dir).r;
}"""


def _gen_shadow_sampler_hlsl() -> str:
    """Generate switch-based shadow sampling for DX11/DX12.
    Uses [branch] not [flatten] — [flatten] evaluates ALL 8 texture samples
    per call which causes catastrophic GPU load inside PCF loops (8×49=392
    samples per shadow calc × multiple lights → GPU hang)."""
    return """\
Texture2D u_ShadowMaps[8] : register(t4);
TextureCube u_ShadowCubeMaps[8] : register(t12);
SamplerState u_ShadowSampler : register(s8);

float sampleShadowMap(int idx, float2 uv) {
    // SampleLevel with explicit LOD 0 — shadow maps are single-mip depth textures.
    // Using Sample() inside a dynamic branch is undefined behavior (broken gradients).
    float r = 1.0;
    if (idx == 0) r = u_ShadowMaps[0].SampleLevel(u_ShadowSampler, uv, 0).r;
    else if (idx == 1) r = u_ShadowMaps[1].SampleLevel(u_ShadowSampler, uv, 0).r;
    else if (idx == 2) r = u_ShadowMaps[2].SampleLevel(u_ShadowSampler, uv, 0).r;
    else if (idx == 3) r = u_ShadowMaps[3].SampleLevel(u_ShadowSampler, uv, 0).r;
    else if (idx == 4) r = u_ShadowMaps[4].SampleLevel(u_ShadowSampler, uv, 0).r;
    else if (idx == 5) r = u_ShadowMaps[5].SampleLevel(u_ShadowSampler, uv, 0).r;
    else if (idx == 6) r = u_ShadowMaps[6].SampleLevel(u_ShadowSampler, uv, 0).r;
    else if (idx == 7) r = u_ShadowMaps[7].SampleLevel(u_ShadowSampler, uv, 0).r;
    return r;
}
float sampleShadowCubeMap(int idx, float3 dir) {
    float r = 1.0;
    if (idx == 0) r = u_ShadowCubeMaps[0].SampleLevel(u_ShadowSampler, dir, 0).r;
    else if (idx == 1) r = u_ShadowCubeMaps[1].SampleLevel(u_ShadowSampler, dir, 0).r;
    else if (idx == 2) r = u_ShadowCubeMaps[2].SampleLevel(u_ShadowSampler, dir, 0).r;
    else if (idx == 3) r = u_ShadowCubeMaps[3].SampleLevel(u_ShadowSampler, dir, 0).r;
    else if (idx == 4) r = u_ShadowCubeMaps[4].SampleLevel(u_ShadowSampler, dir, 0).r;
    else if (idx == 5) r = u_ShadowCubeMaps[5].SampleLevel(u_ShadowSampler, dir, 0).r;
    else if (idx == 6) r = u_ShadowCubeMaps[6].SampleLevel(u_ShadowSampler, dir, 0).r;
    else if (idx == 7) r = u_ShadowCubeMaps[7].SampleLevel(u_ShadowSampler, dir, 0).r;
    return r;
}"""


def _gen_shadow_sampler_metal() -> str:
    """Generate Metal shadow sampling with texture parameters."""
    return """\
float sampleShadowMap2D(int idx,
    float2 uv,
    texture2d<float> sm0, texture2d<float> sm1, texture2d<float> sm2, texture2d<float> sm3,
    texture2d<float> sm4, texture2d<float> sm5, texture2d<float> sm6, texture2d<float> sm7,
    sampler s) {
    float r = 1.0;
    switch(idx) {
        case 0: r = sm0.sample(s, uv).r; break;
        case 1: r = sm1.sample(s, uv).r; break;
        case 2: r = sm2.sample(s, uv).r; break;
        case 3: r = sm3.sample(s, uv).r; break;
        case 4: r = sm4.sample(s, uv).r; break;
        case 5: r = sm5.sample(s, uv).r; break;
        case 6: r = sm6.sample(s, uv).r; break;
        case 7: r = sm7.sample(s, uv).r; break;
    }
    return r;
}
float sampleShadowCubeM(int idx,
    float3 dir,
    texturecube<float> cm0, texturecube<float> cm1, texturecube<float> cm2, texturecube<float> cm3,
    texturecube<float> cm4, texturecube<float> cm5, texturecube<float> cm6, texturecube<float> cm7,
    sampler s) {
    float r = 1.0;
    switch(idx) {
        case 0: r = cm0.sample(s, dir).r; break;
        case 1: r = cm1.sample(s, dir).r; break;
        case 2: r = cm2.sample(s, dir).r; break;
        case 3: r = cm3.sample(s, dir).r; break;
        case 4: r = cm4.sample(s, dir).r; break;
        case 5: r = cm5.sample(s, dir).r; break;
        case 6: r = cm6.sample(s, dir).r; break;
        case 7: r = cm7.sample(s, dir).r; break;
    }
    return r;
}"""


# Shadow calculation functions in GLSL-like syntax.
# These get run through the body transformer for each backend.
_SHADOW_CALC_FUNCTIONS = """\
float calculateShadowPCF(mat4 lsMatrix, int smIndex, vec3 wPos, vec3 N, vec3 lDir,
                         float bias, float nBias, float blur, float opacity, float res) {
    vec4 lsPos = MUL(lsMatrix, vec4(wPos, 1.0));
    vec3 proj = lsPos.xyz / lsPos.w;
    proj = proj * 0.5 + 0.5;
    if (proj.z > 1.0 || proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0)
        return 1.0;
    float curDepth = proj.z;
    float NdL = max(dot(N, lDir), 0.0);
    float fBias = mix(bias * 2.0, bias, NdL);
    float shadow = 0.0;
    vec2 texelSz = 1.0 / vec2(res, res);
    int kr = clamp(int(blur), 1, 3);
    int sc = 0;
    for (int x = -kr; x <= kr; ++x) {
        for (int y = -kr; y <= kr; ++y) {
            vec2 off = vec2(float(x), float(y)) * texelSz * blur;
            float d = sampleShadowMap(smIndex, proj.xy + off);
            shadow += curDepth - fBias > d ? 1.0 : 0.0;
            sc++;
        }
    }
    shadow /= float(sc);
    shadow *= opacity;
    return 1.0 - shadow;
}

float calculateShadowCube(int cmIndex, vec3 lPos, vec3 wPos, vec3 N,
                          float bias, float blur, float opacity, float lRange) {
    vec3 frag2light = wPos - lPos;
    float curDepth = length(frag2light);
    float normDepth = curDepth / lRange;
    vec3 sDir = normalize(frag2light);
    float closest = sampleShadowCubeMap(cmIndex, sDir);
    vec3 lDir2 = -sDir;
    float NdL = max(dot(N, lDir2), 0.0);
    float nBias = bias / lRange;
    float fBias = mix(nBias * 3.0, nBias * 0.5, NdL);
    float shadow = (normDepth - fBias) > closest ? 1.0 : 0.0;
    shadow *= opacity;
    return 1.0 - shadow;
}

float calculateShadow(int smIndex, vec3 wPos, vec3 N, vec3 lDir, vec3 lPos) {
    if (smIndex < 0 || smIndex >= 8) return 1.0;
    ShadowMap sm = u_Lights.shadowMaps[smIndex];
    bool isCube = sm.shadowParams2.y > 0.5;
    if (isCube) {
        float lRange = sm.shadowParams2.z;
        return calculateShadowCube(smIndex, lPos, wPos, N,
            sm.shadowParams.x, sm.shadowParams.z, sm.shadowParams.w, lRange);
    } else {
        return calculateShadowPCF(sm.lightSpaceMatrix, smIndex, wPos, N, lDir,
            sm.shadowParams.x, sm.shadowParams.y, sm.shadowParams.z,
            sm.shadowParams.w, sm.shadowParams2.x);
    }
}"""

_SHADOW_CALC_METAL = """\
float calculateShadowPCF(float4x4 lsMatrix, int smIndex, float3 wPos, float3 N, float3 lDir,
                         float bias, float nBias, float blur, float opacity, float res,
                         texture2d<float> sm0, texture2d<float> sm1, texture2d<float> sm2, texture2d<float> sm3,
                         texture2d<float> sm4, texture2d<float> sm5, texture2d<float> sm6, texture2d<float> sm7,
                         sampler shadowSampler) {
    float4 lsPos = (lsMatrix * float4(wPos, 1.0));
    float3 proj = lsPos.xyz / lsPos.w;
    proj = proj * 0.5 + 0.5;
    if (proj.z > 1.0 || proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0)
        return 1.0;
    float curDepth = proj.z;
    float NdL = max(dot(N, lDir), 0.0);
    float fBias = mix(bias * 2.0, bias, NdL);
    float shadow = 0.0;
    float2 texelSz = 1.0 / float2(res, res);
    int kr = clamp(int(blur), 1, 3);
    int sc = 0;
    for (int x = -kr; x <= kr; ++x) {
        for (int y = -kr; y <= kr; ++y) {
            float2 off = float2(float(x), float(y)) * texelSz * blur;
            float d = sampleShadowMap2D(smIndex, proj.xy + off, sm0, sm1, sm2, sm3, sm4, sm5, sm6, sm7, shadowSampler);
            shadow += curDepth - fBias > d ? 1.0 : 0.0;
            sc++;
        }
    }
    shadow /= float(sc);
    shadow *= opacity;
    return 1.0 - shadow;
}

float calculateShadowCube(int cmIndex, float3 lPos, float3 wPos, float3 N,
                          float bias, float blur, float opacity, float lRange,
                          texturecube<float> cm0, texturecube<float> cm1, texturecube<float> cm2, texturecube<float> cm3,
                          texturecube<float> cm4, texturecube<float> cm5, texturecube<float> cm6, texturecube<float> cm7,
                          sampler shadowSampler) {
    float3 frag2light = wPos - lPos;
    float curDepth = length(frag2light);
    float normDepth = curDepth / lRange;
    float3 sDir = normalize(frag2light);
    float closest = sampleShadowCubeM(cmIndex, sDir, cm0, cm1, cm2, cm3, cm4, cm5, cm6, cm7, shadowSampler);
    float3 lDir2 = -sDir;
    float NdL = max(dot(N, lDir2), 0.0);
    float nBias = bias / lRange;
    float fBias = mix(nBias * 3.0, nBias * 0.5, NdL);
    float shadow = (normDepth - fBias) > closest ? 1.0 : 0.0;
    shadow *= opacity;
    return 1.0 - shadow;
}

float calculateShadow(int smIndex, float3 wPos, float3 N, float3 lDir, float3 lPos,
                      constant LightUniformBuffer& lightData,
                      texture2d<float> sm0, texture2d<float> sm1, texture2d<float> sm2, texture2d<float> sm3,
                      texture2d<float> sm4, texture2d<float> sm5, texture2d<float> sm6, texture2d<float> sm7,
                      texturecube<float> cm0, texturecube<float> cm1, texturecube<float> cm2, texturecube<float> cm3,
                      texturecube<float> cm4, texturecube<float> cm5, texturecube<float> cm6, texturecube<float> cm7,
                      sampler shadowSampler) {
    if (smIndex < 0 || smIndex >= 8) return 1.0;
    ShadowMapData sm = lightData.shadowMaps[smIndex];
    bool isCube = sm.shadowParams2.y > 0.5;
    if (isCube) {
        float lRange = sm.shadowParams2.z;
        return calculateShadowCube(smIndex, lPos, wPos, N,
            sm.shadowParams.x, sm.shadowParams.z, sm.shadowParams.w, lRange,
            cm0, cm1, cm2, cm3, cm4, cm5, cm6, cm7, shadowSampler);
    } else {
        return calculateShadowPCF(sm.lightSpaceMatrix, smIndex, wPos, N, lDir,
            sm.shadowParams.x, sm.shadowParams.y, sm.shadowParams.z,
            sm.shadowParams.w, sm.shadowParams2.x,
            sm0, sm1, sm2, sm3, sm4, sm5, sm6, sm7, shadowSampler);
    }
}"""

# Primary directional-shadow attenuation helper (Gap A). Recovers Godot's
# "unshaded x directional shadow" trick: returns the shadow factor (1 = fully lit,
# 0 = fully shadowed) of the first shadow-casting directional light. Requires
# #feature shadows. GLSL form is run through the body transformer (GL/Vulkan/HLSL);
# Metal needs the explicit texture/sampler/lightData parameter list.
_PRIMARY_SHADOW_GLSL = """\
float lupine_primary_shadow(vec3 wPos, vec3 N) {
    int lpsCount = int(u_Lights.lightCounts.x);
    for (int i = 0; i < 16; ++i) {
        if (i >= lpsCount) break;
        if (int(u_Lights.lights[i].flags.x) != 0) continue;
        if (u_Lights.lights[i].flags.y <= 0.5) continue;
        vec3 L = normalize(-u_Lights.lights[i].direction.xyz);
        return calculateShadow(int(u_Lights.lights[i].flags.z), wPos, N, L,
            u_Lights.lights[i].positionOrDirection.xyz);
    }
    return 1.0;
}"""

_PRIMARY_SHADOW_METAL = """\
float lupine_primary_shadow(float3 wPos, float3 N,
    constant LightUniformBuffer& lightData,
    texture2d<float> sm0, texture2d<float> sm1, texture2d<float> sm2, texture2d<float> sm3,
    texture2d<float> sm4, texture2d<float> sm5, texture2d<float> sm6, texture2d<float> sm7,
    texturecube<float> cm0, texturecube<float> cm1, texturecube<float> cm2, texturecube<float> cm3,
    texturecube<float> cm4, texturecube<float> cm5, texturecube<float> cm6, texturecube<float> cm7,
    sampler shadowSampler) {
    int lpsCount = int(lightData.lightCounts.x);
    for (int i = 0; i < 16; ++i) {
        if (i >= lpsCount) break;
        if (int(lightData.lights[i].flags.x) != 0) continue;
        if (lightData.lights[i].flags.y <= 0.5) continue;
        float3 L = normalize(-lightData.lights[i].direction.xyz);
        return calculateShadow(int(lightData.lights[i].flags.z), wPos, N, L,
            lightData.lights[i].positionOrDirection.xyz, lightData,
            sm0, sm1, sm2, sm3, sm4, sm5, sm6, sm7,
            cm0, cm1, cm2, cm3, cm4, cm5, cm6, cm7, shadowSampler);
    }
    return 1.0;
}"""


# Fog calculation in GLSL-like syntax (run through the body transformer per backend).
# Reads fogColor/fogParams from the light UBO:
#   fogColor.rgb = color, fogColor.w = enabled flag
#   fogParams = (density, start, end, mode) with mode 0=linear, 1=exp, 2=exp2
_FOG_CALC_FUNCTIONS = """\
vec3 applyFog(vec3 color, vec3 wPos, vec3 camPos) {
    if (u_Lights.fogColor.w < 0.5) {
        return color;
    }
    float dist = length(wPos - camPos);
    float density = u_Lights.fogParams.x;
    float fogStart = u_Lights.fogParams.y;
    float fogEnd = u_Lights.fogParams.z;
    int mode = int(u_Lights.fogParams.w);
    float factor = 1.0;
    if (mode == 0) {
        factor = clamp((fogEnd - dist) / max(fogEnd - fogStart, 0.0001), 0.0, 1.0);
    } else if (mode == 1) {
        factor = exp(-density * max(dist - fogStart, 0.0));
    } else {
        float dd = density * max(dist - fogStart, 0.0);
        factor = exp(-dd * dd);
    }
    return mix(u_Lights.fogColor.rgb, color, clamp(factor, 0.0, 1.0));
}"""

_FOG_CALC_METAL = """\
float3 applyFog(float3 color, float3 wPos, float3 camPos,
                constant LightUniformBuffer& lightData) {
    if (lightData.fogColor.w < 0.5) {
        return color;
    }
    float dist = length(wPos - camPos);
    float density = lightData.fogParams.x;
    float fogStart = lightData.fogParams.y;
    float fogEnd = lightData.fogParams.z;
    int mode = int(lightData.fogParams.w);
    float factor = 1.0;
    if (mode == 0) {
        factor = clamp((fogEnd - dist) / max(fogEnd - fogStart, 0.0001), 0.0, 1.0);
    } else if (mode == 1) {
        factor = exp(-density * max(dist - fogStart, 0.0));
    } else {
        float dd = density * max(dist - fogStart, 0.0);
        factor = exp(-dd * dd);
    }
    return mix(lightData.fogColor.rgb, color, clamp(factor, 0.0, 1.0));
}"""


# Normal mapping functions — per backend because HLSL/Metal need input params
def _gen_normal_map_function(backend: 'Backend') -> str:
    """Generate getNormalFromMap for a specific backend."""
    if backend in (Backend.DIRECTX11, Backend.DIRECTX12):
        return """\
float3 getNormalFromMap(PS_INPUT input) {
    if (u_TextureFlags.z < 0.5)
        return normalize(input.v_Normal);
    float3 tNorm = u_NormalTexture.Sample(u_NormalTexture_sampler, input.v_TexCoord).xyz * 2.0 - 1.0;
    tNorm.xy *= u_MaterialParams1.z;
    float3 Q1 = ddx(input.v_WorldPos);
    float3 Q2 = ddy(input.v_WorldPos);
    float2 st1 = ddx(input.v_TexCoord);
    float2 st2 = ddy(input.v_TexCoord);
    float3 Nn = normalize(input.v_Normal);
    float3 T = normalize(Q1 * st2.y - Q2 * st1.y);
    float3 B = -normalize(cross(Nn, T));
    float3x3 TBN = float3x3(T, B, Nn);
    return normalize(mul(TBN, tNorm));
}"""
    elif backend == Backend.METAL:
        return """\
float3 getNormalFromMap(VertexOut in, constant MaterialUniforms& uniforms,
                        texture2d<float> u_NormalTexture, sampler u_NormalTexture_sampler) {
    if (uniforms.u_TextureFlags.z < 0.5)
        return normalize(in.v_Normal);
    float3 tNorm = u_NormalTexture.sample(u_NormalTexture_sampler, in.v_TexCoord).xyz * 2.0 - 1.0;
    tNorm.xy *= uniforms.u_MaterialParams1.z;
    float3 Q1 = dfdx(in.v_WorldPos);
    float3 Q2 = dfdy(in.v_WorldPos);
    float2 st1 = dfdx(in.v_TexCoord);
    float2 st2 = dfdy(in.v_TexCoord);
    float3 Nn = normalize(in.v_Normal);
    float3 T = normalize(Q1 * st2.y - Q2 * st1.y);
    float3 B = -normalize(cross(Nn, T));
    float3x3 TBN = float3x3(T, B, Nn);
    return normalize(TBN * tNorm);
}"""
    elif backend == Backend.VULKAN:
        # Vulkan: material uniforms need material. prefix
        return """\
vec3 getNormalFromMap() {
    if (material.u_TextureFlags.z < 0.5)
        return normalize(v_Normal);
    vec3 tNorm = texture(u_NormalTexture, v_TexCoord).xyz * 2.0 - 1.0;
    tNorm.xy *= material.u_MaterialParams1.z;
    vec3 Q1 = dFdx(v_WorldPos);
    vec3 Q2 = dFdy(v_WorldPos);
    vec2 st1 = dFdx(v_TexCoord);
    vec2 st2 = dFdy(v_TexCoord);
    vec3 Nn = normalize(v_Normal);
    vec3 T = normalize(Q1 * st2.y - Q2 * st1.y);
    vec3 B = -normalize(cross(Nn, T));
    mat3 TBN = mat3(T, B, Nn);
    return normalize(TBN * tNorm);
}"""
    else:
        # GLSL (OpenGL, WebGL) - uses uniforms directly
        return """\
vec3 getNormalFromMap() {
    if (u_TextureFlags.z < 0.5)
        return normalize(v_Normal);
    vec3 tNorm = texture(u_NormalTexture, v_TexCoord).xyz * 2.0 - 1.0;
    tNorm.xy *= u_MaterialParams1.z;
    vec3 Q1 = dFdx(v_WorldPos);
    vec3 Q2 = dFdy(v_WorldPos);
    vec2 st1 = dFdx(v_TexCoord);
    vec2 st2 = dFdy(v_TexCoord);
    vec3 Nn = normalize(v_Normal);
    vec3 T = normalize(Q1 * st2.y - Q2 * st1.y);
    vec3 B = -normalize(cross(Nn, T));
    mat3 TBN = mat3(T, B, Nn);
    return normalize(TBN * tNorm);
}"""


def _gen_srgb_helper(backend: 'Backend') -> str:
    """sRGB->linear decode helper for @source_color samplers (Gap G). The engine loads
    custom-shader textures as UNORM (not _SRGB), so the decode must happen in the shader.
    Uses the gamma-2.2 approximation that matches the engine's gamma convention; alpha is
    left linear."""
    if backend in (Backend.DIRECTX11, Backend.DIRECTX12):
        return ('float4 lupine_srgb_to_linear(float4 c) {\n'
                '    return float4(pow(max(c.rgb, float3(0.0, 0.0, 0.0)), '
                'float3(2.2, 2.2, 2.2)), c.a);\n}')
    elif backend == Backend.METAL:
        return ('float4 lupine_srgb_to_linear(float4 c) {\n'
                '    return float4(pow(max(c.rgb, float3(0.0)), float3(2.2)), c.a);\n}')
    else:
        return ('vec4 lupine_srgb_to_linear(vec4 c) {\n'
                '    return vec4(pow(max(c.rgb, vec3(0.0)), vec3(2.2)), c.a);\n}')


class LightingCodeGen:
    """Generates per-backend lighting infrastructure code."""

    @staticmethod
    def get_light_structs(backend: 'Backend') -> str:
        if backend in (Backend.DIRECTX11, Backend.DIRECTX12):
            return _LIGHT_STRUCTS_HLSL
        elif backend == Backend.METAL:
            return _LIGHT_STRUCTS_METAL
        else:
            return _LIGHT_STRUCTS_GLSL

    @staticmethod
    def get_ubo_declaration(backend: 'Backend') -> str:
        if backend == Backend.OPENGL:
            return f'layout(std140) uniform LightData {{\n{_LIGHT_UBO_FIELDS_GLSL}\n}} u_Lights;'
        elif backend == Backend.WEBGL:
            return f'layout(std140) uniform LightData {{\n{_LIGHT_UBO_FIELDS_GLSL}\n}} u_Lights;'
        elif backend == Backend.VULKAN:
            return f'layout(std140, set = 0, binding = 3) uniform LightData {{\n{_LIGHT_UBO_FIELDS_GLSL}\n}} u_Lights;'
        elif backend in (Backend.DIRECTX11, Backend.DIRECTX12):
            return f'cbuffer LightData : register(b3)\n{{\n{_LIGHT_UBO_FIELDS_HLSL}\n}};'
        elif backend == Backend.METAL:
            return ''  # Metal uses struct + function param, no UBO declaration
        return ''

    @staticmethod
    def get_shadow_samplers(backend: 'Backend') -> str:
        if backend == Backend.OPENGL:
            return _gen_shadow_sampler_gl(8, 8)
        elif backend == Backend.WEBGL:
            return _gen_shadow_sampler_gl(4, 4)
        elif backend == Backend.VULKAN:
            return _gen_shadow_sampler_vulkan()
        elif backend in (Backend.DIRECTX11, Backend.DIRECTX12):
            return _gen_shadow_sampler_hlsl()
        elif backend == Backend.METAL:
            return _gen_shadow_sampler_metal()
        return ''

    @staticmethod
    def _shadow_proj_remap(zero_to_one_depth: bool, flip_y: bool) -> str:
        """Build the light-space projection remap statements for a backend.

        Two independent backend conventions drive shadow sampling:

        * Depth range: zero-to-one-depth backends (DirectX/Vulkan/Metal) already
          produce proj.z in [0,1], so only xy is remapped to [0,1] texture space.
          Negative-one-to-one backends (OpenGL/WebGL) remap all three components.
          Remapping z on a zero-to-one projection pushes the comparison depth into
          [0.5,1] and breaks every shadow test.
        * Texture origin: DirectX and Metal sample from a top-left origin, so the
          shadow UV.y must be flipped. Vulkan renders shadow maps with a negative
          viewport height (the flip already happened during rasterization), and
          OpenGL/WebGL use a bottom-left origin, so neither flips here.
        """
        if zero_to_one_depth:
            stmt = 'proj.xy = proj.xy * 0.5 + 0.5;'
        else:
            stmt = 'proj = proj * 0.5 + 0.5;'
        if flip_y:
            stmt += '\n    proj.y = 1.0 - proj.y;'
        return stmt

    @staticmethod
    def get_shadow_calc_glsl(max_shadows: int = 8, zero_to_one_depth: bool = False,
                             flip_y: bool = False) -> str:
        """Returns shadow calc functions in GLSL-like syntax (run through body transformer)."""
        code = _SHADOW_CALC_FUNCTIONS.replace('smIndex >= 8', f'smIndex >= {max_shadows}')
        return code.replace('proj = proj * 0.5 + 0.5;',
                            LightingCodeGen._shadow_proj_remap(zero_to_one_depth, flip_y))

    @staticmethod
    def get_shadow_calc_metal() -> str:
        """Returns Metal shadow calc functions with explicit texture/sampler/lightData params.

        Metal uses a zero-to-one clip-space depth range and a top-left texture
        origin, so xy is remapped to texture space and UV.y is flipped."""
        return _SHADOW_CALC_METAL.replace('proj = proj * 0.5 + 0.5;',
                                          LightingCodeGen._shadow_proj_remap(True, True))

    @staticmethod
    def get_fog_calc_glsl() -> str:
        """Returns fog calc function in GLSL-like syntax (run through body transformer)."""
        return _FOG_CALC_FUNCTIONS

    @staticmethod
    def get_fog_calc_metal() -> str:
        """Returns Metal fog calc function with explicit lightData param."""
        return _FOG_CALC_METAL

    @staticmethod
    def get_normal_map_code(backend: 'Backend') -> str:
        """Returns getNormalFromMap pre-generated for the specific backend."""
        return _gen_normal_map_function(backend)


# ==============================================================================
# Body Code Transformer
# ==============================================================================

class BodyTransformer:
    """Transforms .lsh body code to a specific backend."""

    def __init__(self, backend: Backend, ir: ShaderIR, stage: str):
        self.backend = backend
        self.ir = ir
        self.stage = stage  # "vertex" or "fragment"

    def transform(self, code: str) -> str:
        # Wrap @source_color sampler reads first; lupine_srgb_to_linear survives all
        # downstream transforms and the inner SAMPLE is expanded normally.
        wrapped = self._wrap_source_color_samples(code)
        added_srgb = wrapped != code
        code = wrapped
        if self.backend in (Backend.DIRECTX11, Backend.DIRECTX12):
            code = self._transform_hlsl(code)
        elif self.backend == Backend.METAL:
            code = self._transform_metal(code)
        elif self.backend == Backend.VULKAN:
            code = self._transform_vulkan(code)
        else:
            # OpenGL / WebGL - mostly pass-through
            code = self._transform_glsl(code)
        # Prepend the sRGB helper definition (already backend-correct) ahead of the
        # transformed body so it precedes void main() and any user pre-main helpers; the
        # HLSL/Metal emitters extract it into their pre-main region automatically.
        if added_srgb:
            code = _gen_srgb_helper(self.backend) + '\n\n' + code
        return code

    def _wrap_source_color_samples(self, code: str) -> str:
        """Wrap SAMPLE*/SAMPLE_LOD/SAMPLE_CUBE reads of @source_color textures in
        lupine_srgb_to_linear(...) (Gap G)."""
        names = set(u.name for u in self.ir.properties
                    if (u.is_texture or u.is_cubemap) and u.source_color)
        if not names:
            return code
        for macro in ('SAMPLE_LOD', 'SAMPLE_CUBE', 'SAMPLE'):
            pattern = re.compile(r'\b' + macro + r'\s*\(')
            search_start = 0
            while True:
                m = pattern.search(code, search_start)
                if not m:
                    break
                paren_open = m.end() - 1
                paren_close = self._find_matching_paren(code, paren_open)
                if paren_close < 0:
                    break
                inner = code[paren_open + 1:paren_close]
                depth = 0
                comma = -1
                for i, ch in enumerate(inner):
                    if ch == '(':
                        depth += 1
                    elif ch == ')':
                        depth -= 1
                    elif ch == ',' and depth == 0:
                        comma = i
                        break
                first_arg = (inner[:comma] if comma >= 0 else inner).strip()
                if first_arg in names:
                    whole = code[m.start():paren_close + 1]
                    replacement = 'lupine_srgb_to_linear(' + whole + ')'
                    code = code[:m.start()] + replacement + code[paren_close + 1:]
                    search_start = m.start() + len('lupine_srgb_to_linear(') + len(whole)
                else:
                    search_start = m.end()
        return code

    def _expand_macros(self, code: str) -> str:
        """Expand cross-platform macros to backend-specific code."""
        backend = self.backend

        # DISCARD
        code = re.sub(r'\bDISCARD\s*;', 'discard;', code)

        # TIME (Gap E): engine-fed per-frame seconds. Injected as the u_Time uniform
        # (see _post_process); rewrite here, before uniform-access prefixing, so the
        # Vulkan/Metal passes treat it like any other material uniform.
        code = re.sub(r'\bTIME\b', 'u_Time', code)

        # FRONT_FACING (Gap D): true for front-facing fragments.
        if backend in (Backend.OPENGL, Backend.WEBGL, Backend.VULKAN):
            code = re.sub(r'\bFRONT_FACING\b', 'gl_FrontFacing', code)
        elif backend in (Backend.DIRECTX11, Backend.DIRECTX12):
            code = re.sub(r'\bFRONT_FACING\b', 'input.lupine_frontFacing', code)
        elif backend == Backend.METAL:
            code = re.sub(r'\bFRONT_FACING\b', 'lupine_frontFacing', code)

        # TEXTURE_SIZE(tex) / TEXTURE_PIXEL_SIZE(tex) (Gap F): texel dimensions.
        code = self._expand_texture_size_macros(code)

        # PRIMARY_SHADOW_ATTENUATION(wPos, N) (Gap A): directional shadow factor. With
        # #feature shadows it routes through lupine_primary_shadow(); without it (no
        # shadow infrastructure) it degrades to fully-lit (1.0).
        if re.search(r'\bPRIMARY_SHADOW_ATTENUATION\b', code):
            if self.ir.has_shadows:
                code = self._expand_two_arg_macro(
                    code, 'PRIMARY_SHADOW_ATTENUATION',
                    lambda a, b: f'lupine_primary_shadow({a}, {b})')
            else:
                code = self._expand_two_arg_macro(
                    code, 'PRIMARY_SHADOW_ATTENUATION', lambda a, b: '1.0')

        # FRAG_COORD
        if backend in (Backend.OPENGL, Backend.WEBGL, Backend.VULKAN):
            code = re.sub(r'\bFRAG_COORD\b', 'gl_FragCoord', code)
        elif backend in (Backend.DIRECTX11, Backend.DIRECTX12):
            code = re.sub(r'\bFRAG_COORD\b', 'input.position', code)
        elif backend == Backend.METAL:
            code = re.sub(r'\bFRAG_COORD\b', 'in.position', code)

        # INSTANCE_ID
        if backend in (Backend.OPENGL, Backend.WEBGL, Backend.VULKAN):
            code = re.sub(r'\bINSTANCE_ID\b', 'gl_InstanceID', code)
        elif backend in (Backend.DIRECTX11, Backend.DIRECTX12):
            code = re.sub(r'\bINSTANCE_ID\b', 'input.instanceID', code)
        elif backend == Backend.METAL:
            code = re.sub(r'\bINSTANCE_ID\b', 'iid', code)

        # VERTEX_ID
        if backend in (Backend.OPENGL, Backend.WEBGL, Backend.VULKAN):
            code = re.sub(r'\bVERTEX_ID\b', 'gl_VertexID', code)
        elif backend in (Backend.DIRECTX11, Backend.DIRECTX12):
            code = re.sub(r'\bVERTEX_ID\b', 'input.vertexID', code)
        elif backend == Backend.METAL:
            code = re.sub(r'\bVERTEX_ID\b', 'vid', code)

        # Geometry shader macros (HLSL-only for now)
        if self.stage == "geometry" and backend in (Backend.DIRECTX11, Backend.DIRECTX12):
            code = re.sub(r'GS_INPUT_POSITION\s*\(\s*(\d+)\s*\)', r'input[\1].position', code)
            code = re.sub(r'GS_INPUT\s*\(\s*(\d+)\s*,\s*(\w+)\s*\)', r'input[\1].\2', code)
            code = re.sub(r'\bGS_POSITION\b', 'output.position', code)
            code = re.sub(r'\bGS_EMIT\s*;', 'outputStream.Append(output);', code)
            code = re.sub(r'\bGS_RESTART\s*;', 'outputStream.RestartStrip();', code)
            # Geometry output variables: v_Color -> output.v_Color, v_LineCoord -> output.v_LineCoord
            if self.ir.geometry:
                for out in self.ir.geometry.outputs:
                    if out.name != 'position':
                        code = re.sub(r'(?<!\.)(?<!\w)\b' + out.name + r'\b\s*=',
                                      f'output.{out.name} =', code)

        return code

    def _expand_sample_macros(self, code: str) -> str:
        """Expand SAMPLE/SAMPLE_LOD/SAMPLE_CUBE macros."""
        backend = self.backend

        if backend in (Backend.OPENGL, Backend.WEBGL, Backend.VULKAN):
            code = re.sub(r'SAMPLE_LOD\s*\(\s*(\w+)\s*,\s*', r'textureLod(\1, ', code)
            code = re.sub(r'SAMPLE_CUBE\s*\(\s*(\w+)\s*,\s*', r'texture(\1, ', code)
            code = re.sub(r'SAMPLE\s*\(\s*(\w+)\s*,\s*', r'texture(\1, ', code)
        elif backend in (Backend.DIRECTX11, Backend.DIRECTX12):
            code = re.sub(r'SAMPLE_LOD\s*\(\s*(\w+)\s*,\s*([^,]+)\s*,\s*([^)]+)\)',
                          r'\1.SampleLevel(\1_sampler, \2, \3)', code)
            code = re.sub(r'SAMPLE_CUBE\s*\(\s*(\w+)\s*,\s*([^)]+)\)',
                          r'\1.Sample(\1_sampler, \2)', code)
            code = re.sub(r'SAMPLE\s*\(\s*(\w+)\s*,\s*([^)]+)\)',
                          r'\1.Sample(\1_sampler, \2)', code)
        elif backend == Backend.METAL:
            code = re.sub(r'SAMPLE_LOD\s*\(\s*(\w+)\s*,\s*([^,]+)\s*,\s*([^)]+)\)',
                          r'\1.sample(\1_sampler, \2, level(\3))', code)
            code = re.sub(r'SAMPLE_CUBE\s*\(\s*(\w+)\s*,\s*([^)]+)\)',
                          r'\1.sample(\1_sampler, \2)', code)
            code = re.sub(r'SAMPLE\s*\(\s*(\w+)\s*,\s*([^)]+)\)',
                          r'\1.sample(\1_sampler, \2)', code)

        return code

    def _expand_mul_macros(self, code: str) -> str:
        """Expand MUL() and MAT3() macros."""
        backend = self.backend

        if backend in (Backend.DIRECTX11, Backend.DIRECTX12):
            code = re.sub(r'MUL\s*\(', 'mul(', code)
            code = self._expand_paren_macro(code, 'MAT3', lambda arg: f'(float3x3){arg}')
        elif backend == Backend.METAL:
            code = self._expand_mul_to_operator(code)
            code = self._expand_paren_macro(code, 'MAT3',
                lambda arg: f'float3x3({arg}[0].xyz, {arg}[1].xyz, {arg}[2].xyz)')
        else:
            # GLSL: MUL(a,b) -> (a * b), MAT3(m) -> mat3(m)
            code = self._expand_mul_to_operator(code)
            code = self._expand_paren_macro(code, 'MAT3', lambda arg: f'mat3({arg})')

        return code

    def _expand_texture_size_macros(self, code: str) -> str:
        """Expand TEXTURE_SIZE(tex) / TEXTURE_PIXEL_SIZE(tex) (Gap F).

        TEXTURE_SIZE  -> texel dimensions of the sampler `tex`.
        TEXTURE_PIXEL_SIZE -> 1.0 / TEXTURE_SIZE (Godot's TEXTURE_PIXEL_SIZE).
        HLSL has no inline size intrinsic, so it routes through the
        lupine_textureSize() helper emitted by the HLSL emitter."""
        backend = self.backend

        def texsize_expr(tex: str) -> str:
            if backend == Backend.METAL:
                return f'float2({tex}.get_width(), {tex}.get_height())'
            elif backend in (Backend.DIRECTX11, Backend.DIRECTX12):
                return f'lupine_textureSize({tex})'
            else:
                return f'vec2(textureSize({tex}, 0))'

        def pixelsize_expr(tex: str) -> str:
            size = texsize_expr(tex)
            one = ('float2(1.0, 1.0)'
                   if backend in (Backend.METAL, Backend.DIRECTX11, Backend.DIRECTX12)
                   else 'vec2(1.0)')
            return f'({one} / {size})'

        code = self._expand_paren_macro(code, 'TEXTURE_PIXEL_SIZE', pixelsize_expr)
        code = self._expand_paren_macro(code, 'TEXTURE_SIZE', texsize_expr)
        return code

    @staticmethod
    def _find_matching_paren(code: str, start: int) -> int:
        """Find the index of the closing paren matching the open paren at start."""
        depth = 0
        for i in range(start, len(code)):
            if code[i] == '(':
                depth += 1
            elif code[i] == ')':
                depth -= 1
                if depth == 0:
                    return i
        return -1

    @staticmethod
    def _expand_mul_to_operator(code: str) -> str:
        """Expand MUL(a, b) -> (a * b), handling nested parens correctly."""
        while True:
            m = re.search(r'\bMUL\s*\(', code)
            if not m:
                break
            start = m.start()
            paren_open = m.end() - 1  # Position of '('
            paren_close = BodyTransformer._find_matching_paren(code, paren_open)
            if paren_close < 0:
                break
            inner = code[paren_open + 1:paren_close]
            # Split on the top-level comma
            depth = 0
            comma_pos = -1
            for i, ch in enumerate(inner):
                if ch == '(':
                    depth += 1
                elif ch == ')':
                    depth -= 1
                elif ch == ',' and depth == 0:
                    comma_pos = i
                    break
            if comma_pos < 0:
                break
            arg1 = inner[:comma_pos].strip()
            arg2 = inner[comma_pos + 1:].strip()
            replacement = f'({arg1} * {arg2})'
            code = code[:start] + replacement + code[paren_close + 1:]
        return code

    @staticmethod
    def _expand_mod_to_floored(code: str) -> str:
        """Expand GLSL mod(x, y) -> (x - y * floor(x / y)).

        GLSL mod is floored (result takes the divisor's sign); HLSL/Metal fmod is
        truncated (dividend's sign). They diverge for negative operands, so a naive
        mod->fmod rename is wrong — e.g. the polygon SDF wraps an angle from
        atan2() which is negative on the lower half, and truncated mod there gives a
        bad angle and jagged edges. Handles nested parens and the top-level comma."""
        while True:
            m = re.search(r'\bmod\s*\(', code)
            if not m:
                break
            start = m.start()
            paren_open = m.end() - 1
            paren_close = BodyTransformer._find_matching_paren(code, paren_open)
            if paren_close < 0:
                break
            inner = code[paren_open + 1:paren_close]
            depth = 0
            comma_pos = -1
            for i, ch in enumerate(inner):
                if ch == '(':
                    depth += 1
                elif ch == ')':
                    depth -= 1
                elif ch == ',' and depth == 0:
                    comma_pos = i
                    break
            if comma_pos < 0:
                break
            x = inner[:comma_pos].strip()
            y = inner[comma_pos + 1:].strip()
            replacement = f'(({x}) - ({y}) * floor(({x}) / ({y})))'
            code = code[:start] + replacement + code[paren_close + 1:]
        return code

    @staticmethod
    def _expand_two_arg_macro(code: str, macro_name: str, transform_fn) -> str:
        """Expand a two-argument paren macro NAME(a, b) with balanced-paren handling."""
        while True:
            m = re.search(r'\b' + macro_name + r'\s*\(', code)
            if not m:
                break
            start = m.start()
            paren_open = m.end() - 1
            paren_close = BodyTransformer._find_matching_paren(code, paren_open)
            if paren_close < 0:
                break
            inner = code[paren_open + 1:paren_close]
            depth = 0
            comma = -1
            for i, ch in enumerate(inner):
                if ch == '(':
                    depth += 1
                elif ch == ')':
                    depth -= 1
                elif ch == ',' and depth == 0:
                    comma = i
                    break
            if comma < 0:
                break
            a = inner[:comma].strip()
            b = inner[comma + 1:].strip()
            code = code[:start] + transform_fn(a, b) + code[paren_close + 1:]
        return code

    @staticmethod
    def _expand_paren_macro(code: str, macro_name: str, transform_fn) -> str:
        """Expand a single-argument paren macro like MAT3(arg)."""
        while True:
            m = re.search(r'\b' + macro_name + r'\s*\(', code)
            if not m:
                break
            start = m.start()
            paren_open = m.end() - 1
            paren_close = BodyTransformer._find_matching_paren(code, paren_open)
            if paren_close < 0:
                break
            arg = code[paren_open + 1:paren_close].strip()
            replacement = transform_fn(arg)
            code = code[:start] + replacement + code[paren_close + 1:]
        return code

    def _replace_types(self, code: str) -> str:
        """Replace GLSL types with backend-specific types."""
        if self.backend in (Backend.DIRECTX11, Backend.DIRECTX12):
            type_map = GLSL_TO_HLSL_TYPES
        elif self.backend == Backend.METAL:
            type_map = GLSL_TO_METAL_TYPES
        else:
            return code

        for glsl_t, target_t in type_map.items():
            if glsl_t in ('sampler2D', 'samplerCube', 'sampler2DShadow'):
                continue  # These are handled specially
            code = re.sub(r'\b' + glsl_t + r'\b', target_t, code)

        # HLSL/Metal: expand scalar broadcast constructors
        # vec3(x) -> float3(x,x,x), vec2(x) -> float2(x,x), vec4(x) -> float4(x,x,x,x)
        if self.backend in (Backend.DIRECTX11, Backend.DIRECTX12, Backend.METAL):
            code = self._expand_scalar_constructors(code)
        return code

    @staticmethod
    def _expand_scalar_constructors(code: str) -> str:
        """Expand HLSL/Metal scalar broadcast: float3(1.0) -> float3(1.0, 1.0, 1.0)"""
        def _expand_match(m):
            type_name = m.group(1)
            arg = m.group(2).strip()
            # Only expand if it's a single scalar argument (no commas)
            if ',' in arg:
                return m.group(0)  # Already multi-arg, leave as-is
            dim = {'float2': 2, 'float3': 3, 'float4': 4,
                    'int2': 2, 'int3': 3, 'int4': 4}.get(type_name)
            if dim:
                return f'{type_name}({", ".join([arg] * dim)})'
            return m.group(0)

        # Match float2/3/4(single_arg) and int2/3/4(single_arg)
        code = re.sub(
            r'\b(float[234]|int[234])\s*\(\s*([^(),]+)\s*\)',
            _expand_match, code)
        return code

    def _replace_functions(self, code: str) -> str:
        """Replace GLSL function names with backend equivalents."""
        if self.backend in (Backend.DIRECTX11, Backend.DIRECTX12):
            replacements = HLSL_FUNC_REPLACEMENTS
        elif self.backend == Backend.METAL:
            replacements = METAL_FUNC_REPLACEMENTS
        else:
            return code

        for glsl_fn, target_fn in replacements.items():
            code = re.sub(r'\b' + glsl_fn + r'\s*\(', target_fn + '(', code)
        return code

    def _transform_uniform_access(self, code: str) -> str:
        """Add prefixes for uniform access in backends that need them."""
        # Transform u_Lights. access for lighting feature
        if self.ir.has_lighting:
            code = self._transform_lights_access(code)

        if self.backend == Backend.VULKAN:
            return self._transform_vulkan_uniforms(code)
        elif self.backend == Backend.METAL:
            return self._transform_metal_uniforms(code)
        elif self.backend in (Backend.DIRECTX11, Backend.DIRECTX12):
            return self._transform_hlsl_uniforms(code)
        return code

    def _transform_lights_access(self, code: str) -> str:
        """Transform u_Lights. access pattern per backend."""
        if self.backend in (Backend.DIRECTX11, Backend.DIRECTX12):
            # HLSL: cbuffer members are global scope — strip u_Lights. prefix
            code = re.sub(r'\bu_Lights\.', '', code)
        elif self.backend == Backend.METAL:
            # Metal: function parameter name
            code = re.sub(r'\bu_Lights\.', 'lightData.', code)
        # OpenGL/Vulkan/WebGL: u_Lights. stays as-is (UBO instance name)
        return code

    # Uniforms that go in the separate bone UBO (must match GLSL450Emitter)
    SEPARATE_UBO_UNIFORMS = {'u_BoneTransforms'}

    def _transform_vulkan_uniforms(self, code: str) -> str:
        """For Vulkan, push constant uniforms need pc. prefix,
        material uniforms need material. prefix,
        bone uniforms need bones. prefix."""
        push_const_names = set()
        material_names = set()
        bone_names = set()
        for u in self.ir.properties:
            if u.is_texture or u.is_cubemap:
                continue
            if u.name in ('u_ViewProjection', 'u_Model', 'u_NormalMatrix', 'u_TintColor'):
                push_const_names.add(u.name)
            elif u.name in self.SEPARATE_UBO_UNIFORMS:
                bone_names.add(u.name)
            else:
                material_names.add(u.name)

        for name in push_const_names:
            code = re.sub(r'(?<![.\w])' + name + r'\b', f'pc.{name}', code)
        for name in bone_names:
            code = re.sub(r'(?<![.\w])' + name + r'\b', f'bones.{name}', code)
        for name in material_names:
            code = re.sub(r'(?<![.\w])' + name + r'\b', f'material.{name}', code)
        return code

    def _transform_metal_uniforms(self, code: str) -> str:
        """For Metal, all uniforms accessed through uniforms. prefix."""
        for u in self.ir.properties:
            if u.is_texture or u.is_cubemap:
                continue
            code = re.sub(r'(?<![.\w])' + u.name + r'\b', f'uniforms.{u.name}', code)
        return code

    def _transform_hlsl_uniforms(self, code: str) -> str:
        """For HLSL, uniforms in cbuffer are accessed directly. No transform needed
        except for bool -> int comparison patterns."""
        return code

    def _transform_io_access(self, code: str) -> str:
        """Transform input/output variable access."""
        # Geometry stage handles I/O via GS macros — skip standard transforms
        if self.stage == "geometry":
            return code

        stage_code = self.ir.vertex if self.stage == "vertex" else self.ir.fragment

        if self.backend in (Backend.DIRECTX11, Backend.DIRECTX12):
            if self.stage == "vertex":
                # Inputs: a_Name -> input.a_Name
                for inp in stage_code.inputs:
                    code = re.sub(r'\b' + inp.name + r'\b', f'input.{inp.name}', code)
                # Outputs: v_Name -> output.v_Name
                for out in stage_code.outputs:
                    code = re.sub(r'\b' + out.name + r'\b', f'output.{out.name}', code)
                # VERTEX_OUTPUT -> output.position
                code = re.sub(r'\bVERTEX_OUTPUT\b', 'output.position', code)
            else:
                # Fragment inputs come from geometry outputs (if present) or vertex outputs
                frag_input_source = self.ir.geometry if self.ir.geometry else self.ir.vertex
                if frag_input_source:
                    for out in frag_input_source.outputs:
                        code = re.sub(r'\b' + out.name + r'\b', f'input.{out.name}', code)
                # Fragment outputs: direct assignment, handled in wrapper
                for out in stage_code.outputs:
                    # FragColor stays as-is, mapped in return statement
                    pass

        elif self.backend == Backend.METAL:
            if self.stage == "vertex":
                for inp in stage_code.inputs:
                    code = re.sub(r'\b' + inp.name + r'\b', f'in.{inp.name}', code)
                for out in stage_code.outputs:
                    code = re.sub(r'\b' + out.name + r'\b', f'out.{out.name}', code)
                code = re.sub(r'\bVERTEX_OUTPUT\b', 'out.position', code)
            else:
                frag_input_source = self.ir.geometry if self.ir.geometry else self.ir.vertex
                if frag_input_source:
                    for out in frag_input_source.outputs:
                        code = re.sub(r'\b' + out.name + r'\b', f'in.{out.name}', code)
                for out in stage_code.outputs:
                    # FragColor -> local, returned at end
                    pass
        else:
            # GLSL: VERTEX_OUTPUT -> gl_Position
            code = re.sub(r'\bVERTEX_OUTPUT\b', 'gl_Position', code)

        return code

    def _transform_glsl(self, code: str) -> str:
        code = self._expand_macros(code)
        code = self._expand_sample_macros(code)
        code = self._expand_mul_macros(code)
        code = self._transform_io_access(code)
        code = self._transform_uniform_access(code)
        return code

    def _transform_vulkan(self, code: str) -> str:
        code = self._expand_macros(code)
        code = self._expand_sample_macros(code)
        code = self._expand_mul_macros(code)
        code = self._transform_io_access(code)
        code = self._transform_uniform_access(code)
        return code

    def _transform_hlsl(self, code: str) -> str:
        code = self._expand_macros(code)
        code = self._expand_sample_macros(code)
        code = self._expand_mul_macros(code)
        code = self._transform_io_access(code)
        code = self._transform_uniform_access(code)
        code = self._replace_types(code)
        code = self._replace_functions(code)
        code = self._expand_mod_to_floored(code)
        # In HLSL, 'const' at global scope is an external constant (defaults to 0).
        # Must use 'static const' for compile-time literal values.
        code = hlsl_static_const(code)
        # For HLSL fragment: getNormalFromMap() -> getNormalFromMap(input)
        if self.stage == "fragment" and self.ir.has_normal_mapping:
            code = re.sub(r'\bgetNormalFromMap\s*\(\s*\)', 'getNormalFromMap(input)', code)
        return code

    def _transform_metal(self, code: str) -> str:
        code = self._expand_macros(code)
        code = self._expand_sample_macros(code)
        code = self._expand_mul_macros(code)
        code = self._transform_io_access(code)
        code = self._transform_uniform_access(code)
        code = self._replace_types(code)
        code = self._replace_functions(code)
        code = self._expand_mod_to_floored(code)
        # For Metal fragment: getNormalFromMap() -> getNormalFromMap(in, uniforms, u_NormalTexture, u_NormalTexture_sampler)
        if self.stage == "fragment" and self.ir.has_normal_mapping:
            code = re.sub(r'\bgetNormalFromMap\s*\(\s*\)',
                          'getNormalFromMap(in, uniforms, u_NormalTexture, u_NormalTexture_sampler)', code)
        return code


# ==============================================================================
# Emitters
# ==============================================================================

class GLSLEmitter:
    """Emits GLSL 330 core (OpenGL) shader code."""

    def emit_vertex(self, ir: ShaderIR) -> str:
        lines = ['', '#version 330 core', '']

        # Vertex inputs
        for inp in ir.vertex.inputs:
            lines.append(f'layout(location = {inp.location}) in {inp.type_name} {inp.name};')
        lines.append('')

        # Uniforms (non-texture)
        for u in ir.properties:
            if not u.is_texture and not u.is_cubemap:
                arr = f'[{u.array_size}]' if u.array_size else ''
                lines.append(f'uniform {u.type_name} {u.name}{arr};')
        lines.append('')

        # Outputs
        for out in ir.vertex.outputs:
            lines.append(f'out {out.type_name} {out.name};')
        lines.append('')

        # Shared code
        if ir.shared_code.strip():
            lines.append(ir.shared_code)
            lines.append('')

        # Body
        transformer = BodyTransformer(Backend.OPENGL, ir, "vertex")
        body = transformer.transform(ir.vertex.body)
        lines.append(body)

        return '\n'.join(lines) + '\n'

    def emit_fragment(self, ir: ShaderIR) -> str:
        lines = ['', '#version 330 core', '']

        # Fragment inputs (from vertex outputs)
        if ir.vertex:
            for out in ir.vertex.outputs:
                lines.append(f'in {out.type_name} {out.name};')
            lines.append('')

        # Uniforms (non-texture)
        for u in ir.properties:
            if not u.is_texture and not u.is_cubemap:
                arr = f'[{u.array_size}]' if u.array_size else ''
                lines.append(f'uniform {u.type_name} {u.name}{arr};')

        # Texture uniforms
        for u in ir.properties:
            if u.is_texture:
                lines.append(f'uniform sampler2D {u.name};')
            elif u.is_cubemap:
                lines.append(f'uniform samplerCube {u.name};')
        lines.append('')

        # Lighting feature: LightData UBO + shadow textures + functions
        if ir.has_lighting:
            lines.append(LightingCodeGen.get_light_structs(Backend.OPENGL))
            lines.append(LightingCodeGen.get_ubo_declaration(Backend.OPENGL))
            lines.append('')
        if ir.has_shadows:
            lines.append(LightingCodeGen.get_shadow_samplers(Backend.OPENGL))
            lines.append('')

        # Fragment outputs
        for out in ir.fragment.outputs:
            lines.append(f'out {out.type_name} {out.name};')
        lines.append('')

        # Shadow calculation functions (transformed through body transformer)
        transformer = BodyTransformer(Backend.OPENGL, ir, "fragment")
        if ir.has_shadows:
            calc_code = transformer.transform(LightingCodeGen.get_shadow_calc_glsl())
            lines.append(calc_code)
            lines.append('')
            if ir.uses_primary_shadow:
                lines.append(transformer.transform(_PRIMARY_SHADOW_GLSL))
                lines.append('')
        if ir.has_fog:
            lines.append(transformer.transform(LightingCodeGen.get_fog_calc_glsl()))
            lines.append('')
        if ir.has_normal_mapping:
            nm_code = LightingCodeGen.get_normal_map_code(Backend.OPENGL)
            lines.append(nm_code)
            lines.append('')

        # Shared code
        if ir.shared_code.strip():
            lines.append(ir.shared_code)
            lines.append('')

        # Body
        body = transformer.transform(ir.fragment.body)
        lines.append(body)

        return '\n'.join(lines) + '\n'


class GLSLESEmitter:
    """Emits GLSL ES 3.0 (WebGL 2.0) shader code."""

    def emit_vertex(self, ir: ShaderIR) -> str:
        lines = ['#version 300 es', '', 'precision highp float;', 'precision highp int;', '']

        for inp in ir.vertex.inputs:
            lines.append(f'layout(location = {inp.location}) in {inp.type_name} {inp.name};')
        lines.append('')

        for u in ir.properties:
            if not u.is_texture and not u.is_cubemap:
                arr = f'[{u.array_size}]' if u.array_size else ''
                lines.append(f'uniform {u.type_name} {u.name}{arr};')
        lines.append('')

        for out in ir.vertex.outputs:
            lines.append(f'out {out.type_name} {out.name};')
        lines.append('')

        if ir.shared_code.strip():
            lines.append(ir.shared_code)
            lines.append('')

        transformer = BodyTransformer(Backend.WEBGL, ir, "vertex")
        body = transformer.transform(ir.vertex.body)
        lines.append(body)

        return '\n'.join(lines) + '\n'

    def emit_fragment(self, ir: ShaderIR) -> str:
        lines = ['#version 300 es', '', 'precision highp float;', 'precision highp int;', '']

        if ir.vertex:
            for out in ir.vertex.outputs:
                lines.append(f'in {out.type_name} {out.name};')
            lines.append('')

        for u in ir.properties:
            if not u.is_texture and not u.is_cubemap:
                arr = f'[{u.array_size}]' if u.array_size else ''
                lines.append(f'uniform {u.type_name} {u.name}{arr};')

        for u in ir.properties:
            if u.is_texture:
                lines.append(f'uniform sampler2D {u.name};')
            elif u.is_cubemap:
                lines.append(f'uniform samplerCube {u.name};')
        lines.append('')

        # Lighting feature
        if ir.has_lighting:
            lines.append(LightingCodeGen.get_light_structs(Backend.WEBGL))
            lines.append(LightingCodeGen.get_ubo_declaration(Backend.WEBGL))
            lines.append('')
        if ir.has_shadows:
            lines.append(LightingCodeGen.get_shadow_samplers(Backend.WEBGL))
            lines.append('')

        for out in ir.fragment.outputs:
            lines.append(f'out {out.type_name} {out.name};')
        lines.append('')

        transformer = BodyTransformer(Backend.WEBGL, ir, "fragment")
        if ir.has_shadows:
            lines.append(transformer.transform(LightingCodeGen.get_shadow_calc_glsl(4)))
            lines.append('')
            if ir.uses_primary_shadow:
                lines.append(transformer.transform(_PRIMARY_SHADOW_GLSL))
                lines.append('')
        if ir.has_fog:
            lines.append(transformer.transform(LightingCodeGen.get_fog_calc_glsl()))
            lines.append('')
        if ir.has_normal_mapping:
            lines.append(LightingCodeGen.get_normal_map_code(Backend.WEBGL))
            lines.append('')

        if ir.shared_code.strip():
            lines.append(ir.shared_code)
            lines.append('')

        body = transformer.transform(ir.fragment.body)
        lines.append(body)

        return '\n'.join(lines) + '\n'


class GLSL450Emitter:
    """Emits GLSL 450 (Vulkan) shader code with push constants and UBOs."""

    PUSH_CONSTANT_UNIFORMS = {
        'u_ViewProjection', 'u_Model', 'u_NormalMatrix', 'u_TintColor'
    }

    # Uniforms that must go in a separate UBO because the engine writes them
    # to a dedicated GPU buffer (e.g., bone transforms at binding 6).
    SEPARATE_UBO_UNIFORMS = {'u_BoneTransforms'}

    def _get_push_constants(self, ir: ShaderIR):
        return [u for u in ir.properties
                if u.name in self.PUSH_CONSTANT_UNIFORMS and not u.is_texture and not u.is_cubemap]

    def _get_material_uniforms(self, ir: ShaderIR):
        return [u for u in ir.properties
                if u.name not in self.PUSH_CONSTANT_UNIFORMS
                and u.name not in self.SEPARATE_UBO_UNIFORMS
                and not u.is_texture and not u.is_cubemap]

    def _get_separate_uniforms(self, ir: ShaderIR):
        return [u for u in ir.properties
                if u.name in self.SEPARATE_UBO_UNIFORMS
                and not u.is_texture and not u.is_cubemap]

    def _get_textures(self, ir: ShaderIR):
        return [u for u in ir.properties if u.is_texture or u.is_cubemap]

    def emit_vertex(self, ir: ShaderIR) -> str:
        lines = ['', '#version 450', '']

        for inp in ir.vertex.inputs:
            lines.append(f'layout(location = {inp.location}) in {inp.type_name} {inp.name};')
        lines.append('')

        # Push constants
        pc = self._get_push_constants(ir)
        if pc:
            lines.append('layout(push_constant) uniform PushConstants {')
            for u in pc:
                arr = f'[{u.array_size}]' if u.array_size else ''
                lines.append(f'    {u.type_name} {u.name}{arr};')
            lines.append('} pc;')
            lines.append('')

        # Material UBO
        mat = self._get_material_uniforms(ir)
        if mat:
            lines.append('layout(set = 0, binding = 2) uniform MaterialData {')
            for u in mat:
                arr = f'[{u.array_size}]' if u.array_size else ''
                lines.append(f'    {u.type_name} {u.name}{arr};')
            lines.append('} material;')
            lines.append('')

        # Separate UBO for large arrays (e.g., bone transforms)
        sep = self._get_separate_uniforms(ir)
        if sep:
            lines.append('layout(std140, set = 0, binding = 1) uniform BoneData {')
            for u in sep:
                arr = f'[{u.array_size}]' if u.array_size else ''
                lines.append(f'    {u.type_name} {u.name}{arr};')
            lines.append('} bones;')
            lines.append('')

        # Outputs
        for i, out in enumerate(ir.vertex.outputs):
            lines.append(f'layout(location = {i}) out {out.type_name} {out.name};')
        lines.append('')

        if ir.shared_code.strip():
            lines.append(ir.shared_code)
            lines.append('')

        transformer = BodyTransformer(Backend.VULKAN, ir, "vertex")
        body = transformer.transform(ir.vertex.body)
        lines.append(body)

        return '\n'.join(lines) + '\n'

    def emit_fragment(self, ir: ShaderIR) -> str:
        lines = ['', '#version 450', '']

        # Fragment inputs
        if ir.vertex:
            for i, out in enumerate(ir.vertex.outputs):
                lines.append(f'layout(location = {i}) in {out.type_name} {out.name};')
            lines.append('')

        # Fragment outputs
        for i, out in enumerate(ir.fragment.outputs):
            lines.append(f'layout(location = {i}) out {out.type_name} {out.name};')
        lines.append('')

        # Push constants
        pc = self._get_push_constants(ir)
        if pc:
            lines.append('layout(push_constant) uniform PushConstants {')
            for u in pc:
                arr = f'[{u.array_size}]' if u.array_size else ''
                lines.append(f'    {u.type_name} {u.name}{arr};')
            lines.append('} pc;')
            lines.append('')

        # Material UBO
        mat = self._get_material_uniforms(ir)
        if mat:
            lines.append('layout(set = 0, binding = 2) uniform MaterialData {')
            for u in mat:
                arr = f'[{u.array_size}]' if u.array_size else ''
                lines.append(f'    {u.type_name} {u.name}{arr};')
            lines.append('} material;')
            lines.append('')

        # Separate UBO for large arrays (e.g., bone transforms)
        sep = self._get_separate_uniforms(ir)
        if sep:
            lines.append('layout(std140, set = 0, binding = 1) uniform BoneData {')
            for u in sep:
                arr = f'[{u.array_size}]' if u.array_size else ''
                lines.append(f'    {u.type_name} {u.name}{arr};')
            lines.append('} bones;')
            lines.append('')

        # Textures
        textures = self._get_textures(ir)
        binding = 4
        for u in textures:
            tex_type = 'samplerCube' if u.is_cubemap else 'sampler2D'
            lines.append(f'layout(set = 0, binding = {binding}) uniform {tex_type} {u.name};')
            binding += 1
        if textures:
            lines.append('')

        # Lighting feature
        if ir.has_lighting:
            lines.append(LightingCodeGen.get_light_structs(Backend.VULKAN))
            lines.append(LightingCodeGen.get_ubo_declaration(Backend.VULKAN))
            lines.append('')
        if ir.has_shadows:
            lines.append(LightingCodeGen.get_shadow_samplers(Backend.VULKAN))
            lines.append('')

        transformer = BodyTransformer(Backend.VULKAN, ir, "fragment")
        if ir.has_shadows:
            lines.append(transformer.transform(LightingCodeGen.get_shadow_calc_glsl(zero_to_one_depth=True)))
            lines.append('')
            if ir.uses_primary_shadow:
                lines.append(transformer.transform(_PRIMARY_SHADOW_GLSL))
                lines.append('')
        if ir.has_fog:
            lines.append(transformer.transform(LightingCodeGen.get_fog_calc_glsl()))
            lines.append('')
        if ir.has_normal_mapping:
            lines.append(LightingCodeGen.get_normal_map_code(Backend.VULKAN))
            lines.append('')

        if ir.shared_code.strip():
            lines.append(ir.shared_code)
            lines.append('')

        body = transformer.transform(ir.fragment.body)
        lines.append(body)

        return '\n'.join(lines) + '\n'


def _extract_pre_main(body: str) -> str:
    """Extract code that appears before void main() in a stage body (helper functions)."""
    lines = body.split('\n')
    result = []
    for line in lines:
        stripped = line.strip()
        if stripped.startswith('void main'):
            break
        result.append(line)
    return '\n'.join(result)


def _append_metal_shadow_args(code: str) -> str:
    """Append Metal shadow texture/sampler/lightData arguments to calculateShadow() and
    lupine_primary_shadow() call sites (both take the same trailing parameter list)."""
    args = (', lightData, '
            + ', '.join(f'shadowMap{i}' for i in range(8)) + ', '
            + ', '.join(f'shadowCubeMap{i}' for i in range(8))
            + ', shadowSampler')
    pattern = re.compile(r'\b(?:calculateShadow|lupine_primary_shadow)\s*\(')
    out = []
    pos = 0
    while True:
        m = pattern.search(code, pos)
        if not m:
            out.append(code[pos:])
            break
        open_paren = m.end() - 1
        close = BodyTransformer._find_matching_paren(code, open_paren)
        if close < 0:
            out.append(code[pos:])
            break
        out.append(code[pos:close])
        out.append(args)
        pos = close
    return ''.join(out)


def _append_metal_fog_args(code: str) -> str:
    """Append the Metal lightData argument to applyFog() calls."""
    pattern = re.compile(r'\bapplyFog\s*\(')
    out = []
    pos = 0
    while True:
        m = pattern.search(code, pos)
        if not m:
            out.append(code[pos:])
            break
        open_paren = m.end() - 1
        close = BodyTransformer._find_matching_paren(code, open_paren)
        if close < 0:
            out.append(code[pos:])
            break
        out.append(code[pos:close])
        out.append(', lightData')
        pos = close
    return ''.join(out)


def _extract_main_body(body: str) -> str:
    """Extract the inner body of void main() { ... }, preserving all inner braces."""
    idx = body.find('void main')
    if idx == -1:
        return ''
    open_idx = body.find('{', idx)
    if open_idx == -1:
        return ''
    depth = 1
    start = open_idx + 1
    i = start
    while i < len(body):
        ch = body[i]
        if ch == '{':
            depth += 1
        elif ch == '}':
            depth -= 1
            if depth == 0:
                return body[start:i].strip('\n')
        i += 1
    return body[start:].strip('\n')


def _rewrite_bare_returns(main_body: str, return_expr: str) -> str:
    """Rewrite an early bare ``return;`` in a value-returning entry's main body into
    ``return <return_expr>;``. GLSL allows a bare return inside a void main() that writes
    its result to an out variable (a common early-out); the HLSL/Metal entry points are
    value-returning, so a bare return there is a compile error ("non-void function 'main'
    should return a value"). Applied to the EXTRACTED MAIN BODY only, so void helper
    functions (whose bare returns are valid) are never touched."""
    return re.sub(r'\breturn\s*;', f'return {return_expr};', main_body)


# HLSL has no inline texture-size intrinsic (GetDimensions writes out-params), so
# TEXTURE_SIZE(tex) routes through this helper. Passing Texture2D by value is valid
# in Shader Model 5.0+ (DX11 and DX12).
_HLSL_TEXTURE_SIZE_HELPER = """\
float2 lupine_textureSize(Texture2D t) {
    float2 d;
    t.GetDimensions(d.x, d.y);
    return d;
}"""


class HLSLEmitter:
    """Emits HLSL shader code for DirectX 11 and DirectX 12."""

    def __init__(self, backend: Backend = Backend.DIRECTX11):
        self.backend = backend

    # Uniforms that must go in a separate cbuffer (register b2) because
    # the engine writes them to a dedicated GPU buffer, not the material buffer.
    SEPARATE_CBUFFER_UNIFORMS = {'u_BoneTransforms'}

    # Standard PerObjectUniforms fields. Declaring ANY of these means the shader is
    # driven by the batch rendering path, where the engine uploads a fixed-layout
    # struct as a raw byte blob via pushConstants: the full 352-byte
    # PerObjectUniforms struct in RenderWorld::executeBatch (unlit 3D primitives,
    # sprites, 2D shapes / UI), or the prefix-compatible ShadowUniforms struct in
    # the shadow pass. Such a shader's b0 cbuffer MUST begin with the full fixed
    # prefix so those raw uploads land at the correct byte offsets. This applies
    # equally to DirectX 11 and DirectX 12 (DX12 additionally relies on the DXC
    # reflection fallback in DXCCompiler.cpp, which hardcodes these offsets).
    # u_ViewProjection is intentionally excluded: it sits at offset 0 in both the
    # full and compact layouts, so a shader that declares only u_ViewProjection
    # (skybox, text) is safely driven purely by reflection-based setUniform* calls
    # and keeps the compact layout.
    FULL_LAYOUT_TRIGGERS = {
        'u_Model', 'u_NormalMatrix', 'u_TintColor', 'u_Color',
        'u_UseTexture', 'u_AlphaCutoff', 'u_UVRect', 'u_TextureFlags',
        'u_MaterialParams1', 'u_MaterialParams2', 'u_CameraPosition',
        'u_AlbedoColor', 'u_EmissiveColor', 'u_ReceiveShadow',
    }

    # Semantic UI/2D-shape uniforms that ALIAS fixed PerObjectUniforms slots. The
    # engine packs their values into these exact byte offsets in
    # RenderWorld::executeBatch and uploads them as the raw pushConstants blob
    # (the GL/WebGL/Vulkan backends mirror this by re-reading the slot by name):
    #   u_CornerRadius / u_GradientParams / u_PolygonParams -> u_TextureFlags slot @240
    #   u_Size                                              -> u_MaterialParams1 slot @256
    # So in HLSL these names MUST be emitted AT the slot offset, not appended after
    # the 352-byte prefix — otherwise the blob can't reach them and the shape renders
    # with garbage parameters (wrong side count, zero size, etc.).
    TEXTUREFLAGS_SLOT_ALIASES = ('u_CornerRadius', 'u_GradientParams', 'u_PolygonParams')

    def _build_cbuffer(self, ir: ShaderIR) -> str:
        """Build cbuffer for HLSL shaders.

        Two modes:
        1. Full PerObjectUniforms mode: For shaders that use the batch rendering
           path (indicated by declaring any FULL_LAYOUT_TRIGGERS field). The
           cbuffer MUST match the engine's 352-byte PerObjectUniforms struct
           exactly. Extra shader-specific uniforms go after the fixed portion.

        2. Compact mode: For shaders rendered purely via individual setUniform*
           calls and that declare none of the standard per-object fields
           (skybox, text). The cbuffer contains only the declared properties in
           declaration order; reflection resolves their offsets.

        Both DX11 and DX12 use the full layout for batch-rendered shaders. DX11
        needs it because RenderWorld::executeBatch uploads the 352-byte
        PerObjectUniforms struct as a raw blob via pushConstants, and the shadow
        pass uploads a prefix-compatible ShadowUniforms blob; a compact cbuffer
        would place fields like u_TintColor/u_UseTexture/u_UVRect at the wrong
        offsets and read garbage. DX12 additionally relies on its DXC reflection
        fallback, which hardcodes the same offsets.

        Bone transforms (u_BoneTransforms) always go in a separate cbuffer
        at register(b2).
        """
        prop_names = {u.name for u in ir.properties if not u.is_texture and not u.is_cubemap}

        # Determine if this shader uses the full PerObjectUniforms layout.
        # DX12 ALWAYS uses the full layout: the engine's bindPipeline sets
        # m_nextUniformOffset = 352 so that dynamic uniform allocation for
        # shader-specific names (u_View, u_SkyboxType, etc.) starts after
        # the prefix. executeBatch also writes the full 352-byte struct via
        # pushConstants. Both require every DX12 cbuffer to start with the
        # standard prefix.
        #
        # DX11 uses the full layout whenever the shader declares a standard
        # per-object field, because those shaders receive the same raw
        # pushConstants upload (PerObjectUniforms from executeBatch, or the
        # prefix-compatible ShadowUniforms from the shadow pass). Only shaders
        # with no standard fields (skybox, text) stay compact.
        if self.backend == Backend.DIRECTX12:
            uses_full_layout = True
        else:
            uses_full_layout = bool(prop_names & self.FULL_LAYOUT_TRIGGERS)

        if uses_full_layout:
            return self._build_cbuffer_full(ir, prop_names)
        else:
            return self._build_cbuffer_compact(ir, prop_names)

    def _build_cbuffer_full(self, ir: ShaderIR, prop_names: set) -> str:
        """Full PerObjectUniforms cbuffer for batch-rendered shaders."""
        # The engine's fixed push constants layout (352 bytes)
        fixed_layout = [
            ('float4x4', 'u_ViewProjection',  64),
            ('float4x4', 'u_Model',           64),
            ('float4x4', 'u_NormalMatrix',     64),
            ('float4',   'u_TintColor',        16),
            ('int',      'u_UseTexture',        4),
            ('float',    'u_AlphaCutoff',       4),
            ('float',    '_pad1',               4),
            ('float',    '_pad2',               4),
            ('float4',   'u_UVRect',           16),
            ('float4',   'u_TextureFlags',     16),
            ('float4',   'u_MaterialParams1',  16),
            ('float4',   'u_MaterialParams2',  16),
            ('float4',   'u_CameraPosition',   16),  # vec3 packed as vec4
            ('float4',   'u_AlbedoColor',      16),
            ('float4',   'u_EmissiveColor',    16),
            ('int',      'u_ReceiveShadow',     4),
            ('float',    '_pad3',               4),
            ('float',    '_pad4',               4),
            ('float',    '_pad5',               4),
        ]

        # Resolve slot aliases: UI/2D shaders declare semantic names (u_PolygonParams,
        # u_Size, u_CornerRadius, ...) that the engine packs into the u_TextureFlags
        # (@240) and u_MaterialParams1 (@256) slots. Emit the alias name at the slot so
        # the pushConstants blob delivers it. A shader that declares the canonical slot
        # name keeps it.
        texflags_alias = None
        if 'u_TextureFlags' not in prop_names:
            texflags_alias = next((a for a in self.TEXTUREFLAGS_SLOT_ALIASES
                                   if a in prop_names), None)
        size_alias = ('u_Size' in prop_names) and ('u_MaterialParams1' not in prop_names)

        lines = ['cbuffer PushConstants : register(b0)', '{']
        emitted = set()
        for hlsl_t, name, _size in fixed_layout:
            if name == 'u_TextureFlags' and texflags_alias:
                lines.append(f'    float4 {texflags_alias};')
                emitted.add(texflags_alias)
                emitted.add(name)
            elif name == 'u_MaterialParams1' and size_alias:
                # u_Size is a float2 occupying the first 8 bytes of the 16-byte slot;
                # pad the rest so u_MaterialParams2 stays at offset 272.
                lines.append('    float2 u_Size;')
                lines.append('    float2 _padSize;')
                emitted.add('u_Size')
                emitted.add(name)
            else:
                lines.append(f'    {hlsl_t} {name};')
                emitted.add(name)
        lines.append('};')

        # Separate out bone transforms and similar large arrays into their own cbuffer
        separate_uniforms = [u for u in ir.properties
                             if not u.is_texture and not u.is_cubemap
                             and u.name in self.SEPARATE_CBUFFER_UNIFORMS
                             and u.name not in emitted]

        if separate_uniforms:
            lines.append('')
            lines.append('cbuffer BoneData : register(b2)')
            lines.append('{')
            for u in separate_uniforms:
                ht = hlsl_type(u.type_name)
                arr = f'[{u.array_size}]' if u.array_size else ''
                lines.append(f'    {ht} {u.name}{arr};')
                emitted.add(u.name)
            lines.append('};')

        # Any remaining non-texture, non-fixed, non-separate properties go AFTER
        # the fixed layout in the SAME cbuffer. The engine uses a 64KB material
        # buffer and setUniform* writes to offsets found via reflection, so extra
        # uniforms after the 352-byte fixed block are accessible.
        remaining = [u for u in ir.properties
                     if not u.is_texture and not u.is_cubemap
                     and u.name not in emitted
                     and u.name not in self.SEPARATE_CBUFFER_UNIFORMS]

        if remaining:
            # Re-open the main cbuffer — insert remaining before the closing brace
            # Find the closing '};' of PushConstants and reopen it
            idx = lines.index('};')
            lines[idx:idx+1] = [
                '    // --- Shader-specific uniforms (set via setUniform*) ---',
                *[f'    {hlsl_type(u.type_name)} {u.name}{"[" + str(u.array_size) + "]" if u.array_size else ""};'
                  for u in remaining],
                '};'
            ]

        return '\n'.join(lines)

    def _build_cbuffer_compact(self, ir: ShaderIR, prop_names: set) -> str:
        """Compact cbuffer for shaders that use individual setUniform* calls.

        Emits declared properties in order. The layout will match both:
        - Shader reflection (DX11 D3DReflect extracts actual offsets)
        - DX12 fallback dynamic allocation (sequential offset assignment)
        """
        lines = ['cbuffer PushConstants : register(b0)', '{']
        emitted = set()

        for u in ir.properties:
            if u.is_texture or u.is_cubemap:
                continue
            if u.name in self.SEPARATE_CBUFFER_UNIFORMS:
                continue
            ht = hlsl_type(u.type_name)
            arr = f'[{u.array_size}]' if u.array_size else ''
            lines.append(f'    {ht} {u.name}{arr};')
            emitted.add(u.name)

        lines.append('};')

        # Separate bone cbuffer if needed
        separate_uniforms = [u for u in ir.properties
                             if not u.is_texture and not u.is_cubemap
                             and u.name in self.SEPARATE_CBUFFER_UNIFORMS]
        if separate_uniforms:
            lines.append('')
            lines.append('cbuffer BoneData : register(b2)')
            lines.append('{')
            for u in separate_uniforms:
                ht = hlsl_type(u.type_name)
                arr = f'[{u.array_size}]' if u.array_size else ''
                lines.append(f'    {ht} {u.name}{arr};')
            lines.append('};')

        return '\n'.join(lines)

    def _emit_textures(self, ir: ShaderIR) -> str:
        lines = []
        slot = 0
        for u in ir.properties:
            if u.is_texture:
                lines.append(f'Texture2D {u.name} : register(t{slot});')
                lines.append(f'SamplerState {u.name}_sampler : register(s{slot});')
                slot += 1
            elif u.is_cubemap:
                lines.append(f'TextureCube {u.name} : register(t{slot});')
                lines.append(f'SamplerState {u.name}_sampler : register(s{slot});')
                slot += 1
        return '\n'.join(lines)

    def emit_vertex(self, ir: ShaderIR) -> str:
        lines = [f'// {self.backend.value} {ir.name} Vertex Shader', '']

        # cbuffer
        lines.append(self._build_cbuffer(ir))
        lines.append('')

        # VS_INPUT
        lines.append('struct VS_INPUT')
        lines.append('{')
        for inp in ir.vertex.inputs:
            ht = hlsl_type(inp.type_name)
            sem = HLSL_SEMANTIC_MAP.get(inp.semantic, inp.semantic)
            lines.append(f'    {ht} {inp.name} : {sem};')
        lines.append('};')
        lines.append('')

        # VS_OUTPUT
        lines.append('struct VS_OUTPUT')
        lines.append('{')
        lines.append('    float4 position : SV_POSITION;')
        texcoord_idx = 0
        for out in ir.vertex.outputs:
            ht = hlsl_type(out.type_name)
            if out.semantic:
                lines.append(f'    {ht} {out.name} : {out.semantic};')
            else:
                lines.append(f'    {ht} {out.name} : TEXCOORD{texcoord_idx};')
                texcoord_idx += 1
        lines.append('};')
        lines.append('')

        # Shared code
        if ir.shared_code.strip():
            transformer = BodyTransformer(self.backend, ir, "vertex")
            shared = transformer._replace_types(ir.shared_code)
            shared = transformer._replace_functions(shared)
            # In HLSL, 'const' at global scope is an external constant (defaults to 0).
            # Must use 'static const' for compile-time literal values.
            shared = hlsl_static_const(shared)
            lines.append(shared)
            lines.append('')

        # Pre-main helper functions from vertex body
        transformer = BodyTransformer(self.backend, ir, "vertex")
        body = transformer.transform(ir.vertex.body)
        if 'lupine_textureSize(' in body:
            lines.append(_HLSL_TEXTURE_SIZE_HELPER)
            lines.append('')
        pre_main = _extract_pre_main(body)
        if pre_main.strip():
            lines.append(pre_main)
            lines.append('')

        # main function
        lines.append('VS_OUTPUT main(VS_INPUT input)')
        lines.append('{')
        lines.append('    VS_OUTPUT output;')
        lines.append('')

        inner = _rewrite_bare_returns(_extract_main_body(body), 'output')
        for bline in inner.split('\n'):
            stripped = bline.strip()
            if stripped:
                lines.append(f'    {stripped}')

        lines.append('')
        lines.append('    return output;')
        lines.append('}')

        return '\n'.join(lines) + '\n'

    def emit_fragment(self, ir: ShaderIR) -> str:
        lines = [f'// {self.backend.value} {ir.name} Fragment Shader', '']

        # cbuffer
        lines.append(self._build_cbuffer(ir))
        lines.append('')

        # Textures
        tex_str = self._emit_textures(ir)
        if tex_str:
            lines.append(tex_str)
            lines.append('')

        # Lighting feature: structs + cbuffer + shadow textures
        if ir.has_lighting:
            lines.append(LightingCodeGen.get_light_structs(self.backend))
            lines.append('')
            lines.append(LightingCodeGen.get_ubo_declaration(self.backend))
            lines.append('')
        if ir.has_shadows:
            lines.append(LightingCodeGen.get_shadow_samplers(self.backend))
            lines.append('')

        # PS_INPUT — use geometry outputs if geometry stage exists, else vertex outputs
        lines.append('struct PS_INPUT')
        lines.append('{')
        lines.append('    float4 position : SV_POSITION;')
        texcoord_idx = 0
        ps_outputs = ir.geometry.outputs if ir.geometry else (ir.vertex.outputs if ir.vertex else [])
        for out in ps_outputs:
            if out.name == 'position' or out.semantic == 'SV_POSITION':
                continue  # Already added above
            ht = hlsl_type(out.type_name)
            if out.semantic:
                lines.append(f'    {ht} {out.name} : {out.semantic};')
            else:
                lines.append(f'    {ht} {out.name} : TEXCOORD{texcoord_idx};')
                texcoord_idx += 1
        if ir.uses_front_facing:
            lines.append('    bool lupine_frontFacing : SV_IsFrontFace;')
        lines.append('};')
        lines.append('')

        # Shadow calculation functions (transformed)
        transformer = BodyTransformer(self.backend, ir, "fragment")
        if ir.has_shadows:
            calc_code = transformer.transform(
                LightingCodeGen.get_shadow_calc_glsl(zero_to_one_depth=True, flip_y=True))
            lines.append(calc_code)
            lines.append('')
            if ir.uses_primary_shadow:
                lines.append(transformer.transform(_PRIMARY_SHADOW_GLSL))
                lines.append('')
        if ir.has_fog:
            lines.append(transformer.transform(LightingCodeGen.get_fog_calc_glsl()))
            lines.append('')
        if ir.has_normal_mapping:
            nm_code = LightingCodeGen.get_normal_map_code(self.backend)
            lines.append(nm_code)
            lines.append('')

        # Shared code
        if ir.shared_code.strip():
            shared = transformer._replace_types(ir.shared_code)
            shared = transformer._replace_functions(shared)
            # In HLSL, 'const' at global scope is an external constant (defaults to 0).
            # Must use 'static const' for compile-time literal values.
            shared = hlsl_static_const(shared)
            lines.append(shared)
            lines.append('')

        # Pre-main helper functions from fragment body
        body = transformer.transform(ir.fragment.body)
        if 'lupine_textureSize(' in body:
            lines.append(_HLSL_TEXTURE_SIZE_HELPER)
            lines.append('')
        pre_main = _extract_pre_main(body)
        if pre_main.strip():
            lines.append(pre_main)
            lines.append('')

        # Determine output type
        out_type = 'float4'
        out_name = 'FragColor'
        if ir.fragment.outputs:
            out_type = hlsl_type(ir.fragment.outputs[0].type_name)
            out_name = ir.fragment.outputs[0].name

        lines.append(f'{out_type} main(PS_INPUT input) : SV_TARGET')
        lines.append('{')

        # Initialize output variable
        lines.append(f'    {out_type} {out_name};')
        lines.append('')

        inner = _rewrite_bare_returns(_extract_main_body(body), out_name)
        for bline in inner.split('\n'):
            stripped = bline.strip()
            if stripped:
                lines.append(f'    {stripped}')

        lines.append('')
        lines.append(f'    return {out_name};')
        lines.append('}')

        return '\n'.join(lines) + '\n'

    def emit_geometry(self, ir: ShaderIR) -> str:
        """Emit HLSL geometry shader."""
        if not ir.geometry:
            return ''

        geo = ir.geometry
        lines = [f'// {self.backend.value} {ir.name} Geometry Shader', '']

        # cbuffer (same as vertex/fragment)
        lines.append(self._build_cbuffer(ir))
        lines.append('')

        # GS_INPUT struct (matches VS_OUTPUT)
        lines.append('struct GS_INPUT')
        lines.append('{')
        lines.append('    float4 position : SV_POSITION;')
        texcoord_idx = 0
        if ir.vertex:
            for out in ir.vertex.outputs:
                ht = hlsl_type(out.type_name)
                if out.semantic:
                    lines.append(f'    {ht} {out.name} : {out.semantic};')
                else:
                    lines.append(f'    {ht} {out.name} : TEXCOORD{texcoord_idx};')
                    texcoord_idx += 1
        lines.append('};')
        lines.append('')

        # GS_OUTPUT struct
        lines.append('struct GS_OUTPUT')
        lines.append('{')
        lines.append('    float4 position : SV_POSITION;')
        texcoord_idx = 0
        for out in geo.outputs:
            if out.name == 'position' or out.semantic == 'SV_POSITION':
                continue
            ht = hlsl_type(out.type_name)
            if out.semantic:
                lines.append(f'    {ht} {out.name} : {out.semantic};')
            else:
                lines.append(f'    {ht} {out.name} : TEXCOORD{texcoord_idx};')
                texcoord_idx += 1
        lines.append('};')
        lines.append('')

        # Topology mapping
        topo_map = {'line': 'line', 'point': 'point', 'triangle': 'triangle'}
        input_topo = topo_map.get(geo.topology, 'line')
        max_verts = geo.max_vertices or 4

        lines.append(f'[maxvertexcount({max_verts})]')
        lines.append(f'void main({input_topo} GS_INPUT input[{2 if input_topo == "line" else 3 if input_topo == "triangle" else 1}], inout TriangleStream<GS_OUTPUT> outputStream)')
        lines.append('{')
        lines.append('    GS_OUTPUT output;')
        lines.append('')

        # Transform geometry body
        transformer = BodyTransformer(self.backend, ir, "geometry")
        body = transformer.transform(ir.geometry.body)
        inner = _extract_main_body(body)
        for bline in inner.split('\n'):
            stripped = bline.strip()
            if stripped:
                lines.append(f'    {stripped}')

        lines.append('}')
        return '\n'.join(lines) + '\n'


class MetalEmitter:
    """Emits Metal Shading Language code."""

    def emit(self, ir: ShaderIR) -> str:
        """Metal shaders have both vertex and fragment in one file."""
        lines = [
            f'// {ir.name} - Metal Shading Language',
            f'// {ir.description}',
            '',
            '#include <metal_stdlib>',
            'using namespace metal;',
            '',
        ]

        # VertexIn struct
        lines.append('struct VertexIn {')
        if ir.vertex:
            for inp in ir.vertex.inputs:
                mt = metal_type(inp.type_name)
                lines.append(f'    {mt} {inp.name} [[attribute({inp.location})]];')
        lines.append('};')
        lines.append('')

        # VertexOut struct
        lines.append('struct VertexOut {')
        lines.append('    float4 position [[position]];')
        if ir.vertex:
            for out in ir.vertex.outputs:
                mt = metal_type(out.type_name)
                lines.append(f'    {mt} {out.name};')
        lines.append('};')
        lines.append('')

        # MaterialUniforms struct
        non_tex_uniforms = [u for u in ir.properties if not u.is_texture and not u.is_cubemap]
        if non_tex_uniforms:
            lines.append('struct MaterialUniforms {')
            for u in non_tex_uniforms:
                mt = metal_type(u.type_name)
                arr = f'[{u.array_size}]' if u.array_size else ''
                lines.append(f'    {mt} {u.name}{arr};')
            lines.append('};')
            lines.append('')

        # Lighting structs (Metal struct definitions at file scope)
        if ir.has_lighting:
            lines.append(LightingCodeGen.get_light_structs(Backend.METAL))
            lines.append('')

        # Shadow sampler free functions (before fragment_main, at file scope)
        if ir.has_shadows:
            lines.append(LightingCodeGen.get_shadow_samplers(Backend.METAL))
            lines.append('')

        # Shared code
        if ir.shared_code.strip():
            transformer = BodyTransformer(Backend.METAL, ir, "vertex")
            shared = transformer._replace_types(ir.shared_code)
            shared = transformer._replace_functions(shared)
            lines.append(shared)
            lines.append('')

        # Vertex function
        if ir.vertex:
            lines.append('vertex VertexOut vertex_main(')
            lines.append('    VertexIn in [[stage_in]],')
            if non_tex_uniforms:
                lines.append('    constant MaterialUniforms& uniforms [[buffer(16)]],')
            lines.append('    uint vid [[vertex_id]],')
            lines.append('    uint iid [[instance_id]]')
            lines.append(') {')
            lines.append('    VertexOut out;')
            lines.append('')

            transformer = BodyTransformer(Backend.METAL, ir, "vertex")
            body = transformer.transform(ir.vertex.body)
            inner = _rewrite_bare_returns(_extract_main_body(body), 'out')
            for bline in inner.split('\n'):
                stripped = bline.strip()
                if stripped:
                    lines.append(f'    {stripped}')

            lines.append('')
            lines.append('    return out;')
            lines.append('}')
            lines.append('')

        # Fragment function
        if ir.fragment:
            # Emit pre-main helper functions before fragment_main
            transformer = BodyTransformer(Backend.METAL, ir, "fragment")
            body = transformer.transform(ir.fragment.body)
            if ir.has_shadows:
                body = _append_metal_shadow_args(body)
            if ir.has_fog:
                body = _append_metal_fog_args(body)

            # Shadow calc + fog + normal map functions (transformed, emitted before fragment_main)
            if ir.has_shadows:
                calc_code = LightingCodeGen.get_shadow_calc_metal()
                lines.append(calc_code)
                lines.append('')
                if ir.uses_primary_shadow:
                    lines.append(_PRIMARY_SHADOW_METAL)
                    lines.append('')
            if ir.has_fog:
                lines.append(LightingCodeGen.get_fog_calc_metal())
                lines.append('')
            if ir.has_normal_mapping:
                nm_code = LightingCodeGen.get_normal_map_code(Backend.METAL)
                lines.append(nm_code)
                lines.append('')

            pre_main = _extract_pre_main(body)
            if pre_main.strip():
                lines.append(pre_main)
                lines.append('')

            lines.append('fragment float4 fragment_main(')
            lines.append('    VertexOut in [[stage_in]],')
            if ir.uses_front_facing:
                lines.append('    bool lupine_frontFacing [[front_facing]],')
            if non_tex_uniforms:
                lines.append('    constant MaterialUniforms& uniforms [[buffer(16)]],')

            # Light buffer parameter
            if ir.has_lighting:
                lines.append('    constant LightUniformBuffer& lightData [[buffer(3)]],')

            # Material textures
            textures = [u for u in ir.properties if u.is_texture or u.is_cubemap]
            for idx, u in enumerate(textures):
                if u.is_cubemap:
                    lines.append(f'    texturecube<float> {u.name} [[texture({idx})]],')
                    lines.append(f'    sampler {u.name}_sampler [[sampler({idx})]],')
                else:
                    lines.append(f'    texture2d<float> {u.name} [[texture({idx})]],')
                    lines.append(f'    sampler {u.name}_sampler [[sampler({idx})]],')

            # Shadow map texture parameters
            if ir.has_shadows:
                tex_start = len(textures)
                for i in range(8):
                    lines.append(f'    texture2d<float> shadowMap{i} [[texture({tex_start + i})]],')
                for i in range(8):
                    lines.append(f'    texturecube<float> shadowCubeMap{i} [[texture({tex_start + 8 + i})]],')
                lines.append(f'    sampler shadowSampler [[sampler({len(textures)})]]')
            else:
                # Remove trailing comma from last param
                if lines[-1].endswith(','):
                    lines[-1] = lines[-1][:-1]

            lines.append(') {')

            out_name = 'FragColor'
            if ir.fragment.outputs:
                out_name = ir.fragment.outputs[0].name
            lines.append(f'    float4 {out_name};')
            lines.append('')

            inner = _rewrite_bare_returns(_extract_main_body(body), out_name)
            for bline in inner.split('\n'):
                stripped = bline.strip()
                if stripped:
                    lines.append(f'    {stripped}')

            lines.append('')
            lines.append(f'    return {out_name};')
            lines.append('}')

        return '\n'.join(lines) + '\n'


# ==============================================================================
# Main Transpiler
# ==============================================================================

class LupineShaderTranspiler:
    """Main transpiler: parses .lsh files and generates backend-specific code."""

    def __init__(self):
        self.parser = LupineShaderParser()

    def transpile(self, source: str, backend: Backend) -> dict:
        """Transpile a .lsh shader to the specified backend.

        Returns a dict with keys depending on backend:
          - OpenGL/Vulkan/WebGL/DX11/DX12: {'vertex': str, 'fragment': str}
          - Metal: {'combined': str}
        """
        ir = self.parser.parse(source)
        return self._emit(ir, backend)

    def transpile_all(self, source: str) -> dict:
        """Transpile to all backends. Returns {backend_name: {files}}."""
        ir = self.parser.parse(source)
        result = {}
        for backend in Backend:
            result[backend.value] = self._emit(ir, backend)
        return result

    def _emit(self, ir: ShaderIR, backend: Backend) -> dict:
        if ir.geometry and backend not in (Backend.DIRECTX11, Backend.DIRECTX12):
            return {}
        if backend == Backend.OPENGL:
            emitter = GLSLEmitter()
            return {
                'vertex': emitter.emit_vertex(ir),
                'fragment': emitter.emit_fragment(ir),
            }
        elif backend == Backend.WEBGL:
            emitter = GLSLESEmitter()
            return {
                'vertex': emitter.emit_vertex(ir),
                'fragment': emitter.emit_fragment(ir),
            }
        elif backend == Backend.VULKAN:
            emitter = GLSL450Emitter()
            return {
                'vertex': emitter.emit_vertex(ir),
                'fragment': emitter.emit_fragment(ir),
            }
        elif backend == Backend.DIRECTX11:
            emitter = HLSLEmitter(Backend.DIRECTX11)
            result = {
                'vertex': emitter.emit_vertex(ir),
                'fragment': emitter.emit_fragment(ir),
            }
            if ir.geometry:
                result['geometry'] = emitter.emit_geometry(ir)
            return result
        elif backend == Backend.DIRECTX12:
            emitter = HLSLEmitter(Backend.DIRECTX12)
            result = {
                'vertex': emitter.emit_vertex(ir),
                'fragment': emitter.emit_fragment(ir),
            }
            if ir.geometry:
                result['geometry'] = emitter.emit_geometry(ir)
            return result
        elif backend == Backend.METAL:
            emitter = MetalEmitter()
            return {
                'combined': emitter.emit(ir),
            }
        return {}


def get_shader_filename(shader_name: str) -> str:
    """Convert shader name to filename convention used by the engine.
    E.g. 'Standard3D' -> 'standard3_d', 'PBR' -> 'pbr',
         'Sprite2D_AlphaCutoff' -> 'sprite2_d_alpha_cutoff'
    """
    result = ''
    for i, c in enumerate(shader_name):
        if c == '_':
            # Keep underscores, but avoid doubling
            if result and not result.endswith('_'):
                result += '_'
            continue
        if c.isupper() and i > 0:
            prev_char = shader_name[i - 1]
            if prev_char == '_':
                pass  # Already have underscore
            elif prev_char.islower() or prev_char.isdigit():
                result += '_'
            elif i + 1 < len(shader_name) and shader_name[i + 1].islower():
                result += '_'
        result += c.lower()
    return result


# ==============================================================================
# CLI
# ==============================================================================

def main():
    import argparse

    parser = argparse.ArgumentParser(description='Lupine Shader Transpiler')
    parser.add_argument('input', help='Input .lsh shader file or directory')
    parser.add_argument('--output', '-o', help='Output directory', default='.')
    parser.add_argument('--backend', '-b', help='Target backend (or "all")', default='all')
    parser.add_argument('--dry-run', action='store_true', help='Print output without writing files')
    args = parser.parse_args()

    transpiler = LupineShaderTranspiler()

    input_files = []
    if os.path.isdir(args.input):
        for f in sorted(os.listdir(args.input)):
            if f.endswith('.lsh'):
                input_files.append(os.path.join(args.input, f))
    else:
        input_files.append(args.input)

    backends = list(Backend) if args.backend == 'all' else [Backend(args.backend)]

    for input_file in input_files:
        with open(input_file, 'r', encoding='utf-8') as f:
            source = f.read()

        ir = transpiler.parser.parse(source)
        filename_base = get_shader_filename(ir.name)
        print(f"Transpiling '{ir.name}' ({os.path.basename(input_file)}) -> {filename_base}")

        for backend in backends:
            result = transpiler._emit(ir, backend)
            if not result:
                continue
            out_dir = os.path.join(args.output, backend.value)
            os.makedirs(out_dir, exist_ok=True)

            if backend == Backend.METAL:
                out_path = os.path.join(out_dir, f'{filename_base}.metal')
                content = result['combined']
                if args.dry_run:
                    print(f"  [{backend.value}] {out_path}")
                else:
                    with open(out_path, 'w', encoding='utf-8') as f:
                        f.write(content)
                    print(f"  [{backend.value}] Wrote {out_path}")
            else:
                ext_map = {'vertex': 'vert', 'fragment': 'frag', 'geometry': 'geom'}
                stages = [s for s in ('vertex', 'fragment', 'geometry') if s in result]
                for stage in stages:
                    ext = ext_map[stage]
                    out_path = os.path.join(out_dir, f'{filename_base}.{ext}')
                    content = result[stage]
                    if args.dry_run:
                        print(f"  [{backend.value}] {out_path}")
                    else:
                        with open(out_path, 'w', encoding='utf-8') as f:
                            f.write(content)
                        print(f"  [{backend.value}] Wrote {out_path}")


if __name__ == '__main__':
    main()
