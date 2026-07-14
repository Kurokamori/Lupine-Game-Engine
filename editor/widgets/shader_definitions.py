"""
Shader Definitions Module

Defines built-in shader types and their material parameters for the editor.
This allows the material override widget to dynamically show/hide parameter
categories based on the selected shader.

Backend Support:
- Shaders are organized by graphics backend (OpenGL, Vulkan, DirectX, Metal, WebGL)
- The registry filters shaders based on the current active backend
- Custom shaders require separate files for each backend they support

Architecture for custom shaders:
- Custom shaders can define their own parameters via a metadata format
- The ShaderParameterDefinition class provides the structure for custom params
- Future: Parse shader files or companion metadata files for parameter info
"""

from dataclasses import dataclass, field
from typing import List, Dict, Optional, Any, Set, Tuple
from enum import Enum
import re
import os


class GraphicsBackend(Enum):
    """Graphics backend types - mirrors lupine::GraphicsBackend in C++"""
    NONE = "None"
    OPENGL = "OpenGL"
    VULKAN = "Vulkan"
    DIRECTX11 = "DirectX11"
    DIRECTX12 = "DirectX12"
    METAL = "Metal"
    WEBGL = "WebGL"

    @classmethod
    def from_engine(cls, engine_backend):
        """Convert engine GraphicsBackend enum to Python enum"""
        try:
            # Get the name from the engine enum value
            name = engine_backend.name if hasattr(engine_backend, 'name') else str(engine_backend)
            # Map to our enum
            mapping = {
                'OpenGL': cls.OPENGL,
                'Vulkan': cls.VULKAN,
                'DirectX11': cls.DIRECTX11,
                'DirectX12': cls.DIRECTX12,
                'Metal': cls.METAL,
                'WebGL': cls.WEBGL,
            }
            return mapping.get(name, cls.OPENGL)
        except Exception:
            return cls.OPENGL

    @property
    def shader_extension(self) -> str:
        """Get file extension for shaders of this backend"""
        extensions = {
            GraphicsBackend.OPENGL: ".glsl",
            GraphicsBackend.WEBGL: ".glsl",
            GraphicsBackend.VULKAN: ".spv",  # SPIR-V compiled
            GraphicsBackend.DIRECTX11: ".hlsl",
            GraphicsBackend.DIRECTX12: ".hlsl",
            GraphicsBackend.METAL: ".metal",
        }
        return extensions.get(self, ".glsl")

    @property
    def shader_folder(self) -> str:
        """Get folder name for shaders of this backend"""
        folders = {
            GraphicsBackend.OPENGL: "OpenGL",
            GraphicsBackend.WEBGL: "WebGL",
            GraphicsBackend.VULKAN: "Vulkan",
            GraphicsBackend.DIRECTX11: "DirectX11",
            GraphicsBackend.DIRECTX12: "DirectX12",
            GraphicsBackend.METAL: "Metal",
        }
        return folders.get(self, "OpenGL")

    @property
    def vertex_shader_name(self) -> str:
        """Get the display name for vertex shader type"""
        names = {
            GraphicsBackend.OPENGL: "Vertex Shader",
            GraphicsBackend.WEBGL: "Vertex Shader",
            GraphicsBackend.VULKAN: "Vertex Shader (SPIR-V/GLSL)",
            GraphicsBackend.DIRECTX11: "Vertex Shader (HLSL)",
            GraphicsBackend.DIRECTX12: "Vertex Shader (HLSL)",
            GraphicsBackend.METAL: "Vertex Function (Metal)",
        }
        return names.get(self, "Vertex Shader")

    @property
    def fragment_shader_name(self) -> str:
        """Get the display name for fragment/pixel shader type"""
        names = {
            GraphicsBackend.OPENGL: "Fragment Shader",
            GraphicsBackend.WEBGL: "Fragment Shader",
            GraphicsBackend.VULKAN: "Fragment Shader (SPIR-V/GLSL)",
            GraphicsBackend.DIRECTX11: "Pixel Shader (HLSL)",
            GraphicsBackend.DIRECTX12: "Pixel Shader (HLSL)",
            GraphicsBackend.METAL: "Fragment Function (Metal)",
        }
        return names.get(self, "Fragment Shader")

    @property
    def vertex_file_placeholder(self) -> str:
        """Get placeholder text for vertex shader file input"""
        placeholders = {
            GraphicsBackend.OPENGL: "Path to vertex shader (.vert)",
            GraphicsBackend.WEBGL: "Path to vertex shader (.vert)",
            GraphicsBackend.VULKAN: "Path to vertex shader (.spv or .vert)",
            GraphicsBackend.DIRECTX11: "Path to vertex shader (.hlsl)",
            GraphicsBackend.DIRECTX12: "Path to vertex shader (.hlsl)",
            GraphicsBackend.METAL: "Path to Metal shader (.metal)",
        }
        return placeholders.get(self, "Path to vertex shader")

    @property
    def fragment_file_placeholder(self) -> str:
        """Get placeholder text for fragment shader file input"""
        placeholders = {
            GraphicsBackend.OPENGL: "Path to fragment shader (.frag)",
            GraphicsBackend.WEBGL: "Path to fragment shader (.frag)",
            GraphicsBackend.VULKAN: "Path to fragment shader (.spv or .frag)",
            GraphicsBackend.DIRECTX11: "Path to pixel shader (.hlsl)",
            GraphicsBackend.DIRECTX12: "Path to pixel shader (.hlsl)",
            GraphicsBackend.METAL: "Path to Metal shader (.metal)",
        }
        return placeholders.get(self, "Path to fragment shader")


class ParameterType(Enum):
    """Types of shader parameters"""
    FLOAT = "float"
    COLOR = "color"          # RGBA color
    TEXTURE = "texture"      # Texture path
    VEC2 = "vec2"
    VEC3 = "vec3"
    VEC4 = "vec4"
    INT = "int"
    BOOL = "bool"


@dataclass
class ShaderParameter:
    """Definition of a single shader parameter"""
    name: str                          # Internal name (e.g., "metallic")
    display_name: str                  # UI display name (e.g., "Metallic")
    param_type: ParameterType          # Type of parameter
    default_value: Any = None          # Default value
    min_value: Optional[float] = None  # For numeric types
    max_value: Optional[float] = None  # For numeric types
    description: str = ""              # Tooltip/description
    # Uniform mapping for modular shader params (used for custom/new shaders)
    uniform_name: Optional[str] = None  # Target uniform name (e.g., "u_GlowParams")
    uniform_component: int = -1         # Component index in Vec4 (0=x, 1=y, 2=z, 3=w), -1 if full value


@dataclass
class ShaderCategory:
    """A category grouping related parameters (e.g., "Albedo", "Emissive")"""
    name: str                          # Category name
    display_name: str                  # UI display name
    parameters: List[ShaderParameter] = field(default_factory=list)


@dataclass
class ShaderDefinition:
    """Complete definition of a shader and its editable parameters"""
    name: str                          # Internal shader name
    display_name: str                  # UI display name
    description: str                   # Description for tooltips
    categories: List[ShaderCategory] = field(default_factory=list)
    supported_backends: Set[GraphicsBackend] = field(default_factory=lambda: {
        GraphicsBackend.OPENGL, GraphicsBackend.WEBGL  # Default: OpenGL/WebGL supported
    })
    is_skeletal: bool = False          # Whether this shader supports skeletal animation
    is_custom: bool = False            # Whether this is a custom/user shader
    custom_paths: Dict[GraphicsBackend, str] = field(default_factory=dict)  # Backend -> shader path

    def supports_backend(self, backend: GraphicsBackend) -> bool:
        """Check if this shader supports the given backend"""
        return backend in self.supported_backends

    def get_custom_path(self, backend: GraphicsBackend) -> str:
        """Get the custom shader path for a specific backend"""
        return self.custom_paths.get(backend, "")


# ============================================================================
# Built-in Shader Definitions
# ============================================================================

def _create_pbr_shader() -> ShaderDefinition:
    """Create PBR shader definition with full material properties"""
    # PBR is the primary material shader - supported on all backends
    all_backends = {
        GraphicsBackend.OPENGL, GraphicsBackend.WEBGL, GraphicsBackend.VULKAN,
        GraphicsBackend.DIRECTX11, GraphicsBackend.DIRECTX12, GraphicsBackend.METAL
    }
    return ShaderDefinition(
        name="PBR",
        display_name="PBR",
        description="Physically-Based Rendering shader with full material support",
        supported_backends=all_backends,
        categories=[
            ShaderCategory(
                name="Albedo",
                display_name="Albedo",
                parameters=[
                    ShaderParameter("albedoColor", "Color", ParameterType.COLOR,
                                  default_value=(1.0, 1.0, 1.0, 1.0)),
                    ShaderParameter("albedoTexture", "Texture", ParameterType.TEXTURE),
                ]
            ),
            ShaderCategory(
                name="MetallicRoughness",
                display_name="Metallic / Roughness",
                parameters=[
                    ShaderParameter("metallic", "Metallic", ParameterType.FLOAT,
                                  default_value=0.0, min_value=0.0, max_value=1.0),
                    ShaderParameter("roughness", "Roughness", ParameterType.FLOAT,
                                  default_value=0.5, min_value=0.0, max_value=1.0),
                    ShaderParameter("metallicRoughnessTexture", "Texture", ParameterType.TEXTURE),
                ]
            ),
            ShaderCategory(
                name="Normal",
                display_name="Normal",
                parameters=[
                    ShaderParameter("normalTexture", "Texture", ParameterType.TEXTURE),
                    ShaderParameter("normalScale", "Scale", ParameterType.FLOAT,
                                  default_value=1.0, min_value=0.0, max_value=2.0),
                ]
            ),
            ShaderCategory(
                name="Emissive",
                display_name="Emissive",
                parameters=[
                    ShaderParameter("emissiveColor", "Color", ParameterType.COLOR,
                                  default_value=(0.0, 0.0, 0.0, 1.0)),
                    ShaderParameter("emissiveTexture", "Texture", ParameterType.TEXTURE),
                    ShaderParameter("emissiveStrength", "Strength", ParameterType.FLOAT,
                                  default_value=1.0, min_value=0.0, max_value=10.0),
                ]
            ),
            ShaderCategory(
                name="Alpha",
                display_name="Alpha",
                parameters=[
                    ShaderParameter("alphaCutoff", "Cutoff", ParameterType.FLOAT,
                                  default_value=0.5, min_value=0.0, max_value=1.0),
                ]
            ),
        ]
    )


def _create_skeletal_shader() -> ShaderDefinition:
    """Create Skeletal PBR shader definition - same as PBR but for animated meshes"""
    # Skeletal shaders supported on all backends
    all_backends = {
        GraphicsBackend.OPENGL, GraphicsBackend.WEBGL, GraphicsBackend.VULKAN,
        GraphicsBackend.DIRECTX11, GraphicsBackend.DIRECTX12, GraphicsBackend.METAL
    }
    shader = _create_pbr_shader()
    shader.name = "Skeletal"
    shader.display_name = "Skeletal PBR"
    shader.description = "Physically-Based Rendering shader with skeletal animation support"
    shader.supported_backends = all_backends
    shader.is_skeletal = True
    return shader


def _create_unlit_shader() -> ShaderDefinition:
    """Create Unlit shader definition - just color/texture, no lighting"""
    # Unlit is a simple shader - supported on all backends
    all_backends = {
        GraphicsBackend.OPENGL, GraphicsBackend.WEBGL, GraphicsBackend.VULKAN,
        GraphicsBackend.DIRECTX11, GraphicsBackend.DIRECTX12, GraphicsBackend.METAL
    }
    return ShaderDefinition(
        name="Unlit",
        display_name="Unlit",
        description="Simple unlit shader - no lighting calculations, just color/texture",
        supported_backends=all_backends,
        categories=[
            ShaderCategory(
                name="Albedo",
                display_name="Color",
                parameters=[
                    ShaderParameter("albedoColor", "Color", ParameterType.COLOR,
                                  default_value=(1.0, 1.0, 1.0, 1.0)),
                    ShaderParameter("albedoTexture", "Texture", ParameterType.TEXTURE),
                ]
            ),
            ShaderCategory(
                name="Alpha",
                display_name="Alpha",
                parameters=[
                    ShaderParameter("alphaCutoff", "Cutoff", ParameterType.FLOAT,
                                  default_value=0.5, min_value=0.0, max_value=1.0),
                ]
            ),
        ]
    )


def _create_standard3d_shader() -> ShaderDefinition:
    """Create Standard3D shader - basic lit shader without PBR features"""
    # Standard3D is a basic shader - supported on all backends
    all_backends = {
        GraphicsBackend.OPENGL, GraphicsBackend.WEBGL, GraphicsBackend.VULKAN,
        GraphicsBackend.DIRECTX11, GraphicsBackend.DIRECTX12, GraphicsBackend.METAL
    }
    return ShaderDefinition(
        name="Standard3D",
        display_name="Standard 3D",
        description="Basic lit shader with diffuse and specular lighting",
        supported_backends=all_backends,
        categories=[
            ShaderCategory(
                name="Albedo",
                display_name="Diffuse",
                parameters=[
                    ShaderParameter("albedoColor", "Color", ParameterType.COLOR,
                                  default_value=(1.0, 1.0, 1.0, 1.0)),
                    ShaderParameter("albedoTexture", "Texture", ParameterType.TEXTURE),
                ]
            ),
            ShaderCategory(
                name="Normal",
                display_name="Normal",
                parameters=[
                    ShaderParameter("normalTexture", "Texture", ParameterType.TEXTURE),
                    ShaderParameter("normalScale", "Scale", ParameterType.FLOAT,
                                  default_value=1.0, min_value=0.0, max_value=2.0),
                ]
            ),
        ]
    )


def _create_toon_shader() -> ShaderDefinition:
    """Create Toon/Cel shader definition with classic cartoon-style shading"""
    # Toon shader - currently implemented for OpenGL/WebGL
    all_backends = {
        GraphicsBackend.OPENGL, GraphicsBackend.WEBGL, GraphicsBackend.VULKAN,
        GraphicsBackend.DIRECTX11, GraphicsBackend.DIRECTX12, GraphicsBackend.METAL
    }
    return ShaderDefinition(
        name="Toon",
        display_name="Toon",
        description="Cel-shaded cartoon-style shader with configurable bands and rim lighting",
        supported_backends=all_backends,
        categories=[
            ShaderCategory(
                name="Albedo",
                display_name="Albedo",
                parameters=[
                    ShaderParameter("albedoColor", "Color", ParameterType.COLOR,
                                  default_value=(1.0, 1.0, 1.0, 1.0)),
                    ShaderParameter("albedoTexture", "Texture", ParameterType.TEXTURE),
                ]
            ),
            ShaderCategory(
                name="ToonSettings",
                display_name="Toon Settings",
                parameters=[
                    ShaderParameter("shadowBands", "Shadow Bands", ParameterType.FLOAT,
                                  default_value=3.0, min_value=2.0, max_value=10.0,
                                  description="Number of discrete shadow bands (2 = classic toon)"),
                    ShaderParameter("shadowThreshold", "Shadow Threshold", ParameterType.FLOAT,
                                  default_value=0.5, min_value=0.0, max_value=1.0,
                                  description="Threshold for shadow cutoff"),
                    ShaderParameter("shadowSoftness", "Shadow Softness", ParameterType.FLOAT,
                                  default_value=0.02, min_value=0.0, max_value=0.2,
                                  description="Softness of shadow band edges"),
                    ShaderParameter("specularBands", "Specular Bands", ParameterType.FLOAT,
                                  default_value=2.0, min_value=1.0, max_value=5.0,
                                  description="Number of specular highlight bands"),
                    ShaderParameter("specularPower", "Specular Power", ParameterType.FLOAT,
                                  default_value=32.0, min_value=1.0, max_value=128.0,
                                  description="Specular highlight sharpness"),
                ]
            ),
            ShaderCategory(
                name="RimLighting",
                display_name="Rim Lighting",
                parameters=[
                    ShaderParameter("rimIntensity", "Intensity", ParameterType.FLOAT,
                                  default_value=0.0, min_value=0.0, max_value=2.0,
                                  description="Rim lighting intensity (0 = disabled)"),
                    ShaderParameter("rimPower", "Power", ParameterType.FLOAT,
                                  default_value=3.0, min_value=0.5, max_value=10.0,
                                  description="Rim lighting falloff power"),
                ]
            ),
            ShaderCategory(
                name="Normal",
                display_name="Normal",
                parameters=[
                    ShaderParameter("normalTexture", "Texture", ParameterType.TEXTURE),
                    ShaderParameter("normalScale", "Scale", ParameterType.FLOAT,
                                  default_value=1.0, min_value=0.0, max_value=2.0),
                ]
            ),
            ShaderCategory(
                name="Emissive",
                display_name="Emissive",
                parameters=[
                    ShaderParameter("emissiveColor", "Color", ParameterType.COLOR,
                                  default_value=(0.0, 0.0, 0.0, 1.0)),
                    ShaderParameter("emissiveTexture", "Texture", ParameterType.TEXTURE),
                    ShaderParameter("emissiveStrength", "Strength", ParameterType.FLOAT,
                                  default_value=1.0, min_value=0.0, max_value=10.0),
                ]
            ),
            ShaderCategory(
                name="Alpha",
                display_name="Alpha",
                parameters=[
                    ShaderParameter("alphaCutoff", "Cutoff", ParameterType.FLOAT,
                                  default_value=0.5, min_value=0.0, max_value=1.0),
                ]
            ),
        ]
    )


def _create_skeletal_toon_shader() -> ShaderDefinition:
    """Create Skeletal Toon shader - toon shading with skeletal animation support"""
    shader = _create_toon_shader()
    shader.name = "SkeletalToon"
    shader.display_name = "Skeletal Toon"
    shader.description = "Cel-shaded cartoon-style shader with skeletal animation support"
    shader.is_skeletal = True
    return shader


def _create_stylized_shader() -> ShaderDefinition:
    """Create Stylized shader definition with fantasy-style soft shading"""
    # Stylized shader - supported on all backends
    all_backends = {
        GraphicsBackend.OPENGL, GraphicsBackend.WEBGL, GraphicsBackend.VULKAN,
        GraphicsBackend.DIRECTX11, GraphicsBackend.DIRECTX12, GraphicsBackend.METAL
    }
    return ShaderDefinition(
        name="Stylized",
        display_name="Stylized",
        description="Fantasy-style shader with soft shadow gradients, shadow ramps, and rim lighting",
        supported_backends=all_backends,
        categories=[
            ShaderCategory(
                name="Albedo",
                display_name="Albedo",
                parameters=[
                    ShaderParameter("albedoColor", "Color", ParameterType.COLOR,
                                  default_value=(1.0, 1.0, 1.0, 1.0)),
                    ShaderParameter("albedoTexture", "Texture", ParameterType.TEXTURE),
                ]
            ),
            ShaderCategory(
                name="StylizedSettings",
                display_name="Stylized Settings",
                parameters=[
                    ShaderParameter("stylizedShadowSoftness", "Shadow Softness", ParameterType.FLOAT,
                                  default_value=0.3, min_value=0.0, max_value=1.0,
                                  description="How soft the shadow transitions are"),
                    ShaderParameter("stylizedShadowBrightness", "Shadow Brightness", ParameterType.FLOAT,
                                  default_value=0.4, min_value=0.0, max_value=1.0,
                                  description="How bright shadows are (0 = dark, 1 = no shadow)"),
                    ShaderParameter("stylizedShadowWarmth", "Shadow Warmth", ParameterType.FLOAT,
                                  default_value=0.5, min_value=0.0, max_value=1.0,
                                  description="Warm/cool color shift in shadows (0 = no shift, 1 = full shift)"),
                    ShaderParameter("stylizedHalfLambertPower", "Half-Lambert Power", ParameterType.FLOAT,
                                  default_value=1.5, min_value=0.5, max_value=4.0,
                                  description="Controls wrap-around lighting softness"),
                ]
            ),
            ShaderCategory(
                name="Specular",
                display_name="Specular",
                parameters=[
                    ShaderParameter("stylizedSpecularSoftness", "Specular Softness", ParameterType.FLOAT,
                                  default_value=0.15, min_value=0.0, max_value=0.5,
                                  description="How soft the specular edge is"),
                    ShaderParameter("stylizedSpecularIntensity", "Specular Intensity", ParameterType.FLOAT,
                                  default_value=0.5, min_value=0.0, max_value=2.0,
                                  description="Specular highlight intensity"),
                    ShaderParameter("specularPower", "Specular Power", ParameterType.FLOAT,
                                  default_value=24.0, min_value=1.0, max_value=128.0,
                                  description="Specular highlight sharpness"),
                ]
            ),
            ShaderCategory(
                name="RimLighting",
                display_name="Rim Lighting",
                parameters=[
                    ShaderParameter("rimIntensity", "Intensity", ParameterType.FLOAT,
                                  default_value=0.3, min_value=0.0, max_value=2.0,
                                  description="Rim lighting intensity (0 = disabled)"),
                    ShaderParameter("rimPower", "Power", ParameterType.FLOAT,
                                  default_value=2.5, min_value=0.5, max_value=10.0,
                                  description="Rim lighting falloff power"),
                ]
            ),
            ShaderCategory(
                name="Normal",
                display_name="Normal",
                parameters=[
                    ShaderParameter("normalTexture", "Texture", ParameterType.TEXTURE),
                    ShaderParameter("normalScale", "Scale", ParameterType.FLOAT,
                                  default_value=1.0, min_value=0.0, max_value=2.0),
                ]
            ),
            ShaderCategory(
                name="Emissive",
                display_name="Emissive",
                parameters=[
                    ShaderParameter("emissiveColor", "Color", ParameterType.COLOR,
                                  default_value=(0.0, 0.0, 0.0, 1.0)),
                    ShaderParameter("emissiveTexture", "Texture", ParameterType.TEXTURE),
                    ShaderParameter("emissiveStrength", "Strength", ParameterType.FLOAT,
                                  default_value=1.0, min_value=0.0, max_value=10.0),
                ]
            ),
            ShaderCategory(
                name="Alpha",
                display_name="Alpha",
                parameters=[
                    ShaderParameter("alphaCutoff", "Cutoff", ParameterType.FLOAT,
                                  default_value=0.5, min_value=0.0, max_value=1.0),
                ]
            ),
        ]
    )


def _create_skeletal_stylized_shader() -> ShaderDefinition:
    """Create Skeletal Stylized shader - stylized shading with skeletal animation support"""
    shader = _create_stylized_shader()
    shader.name = "SkeletalStylized"
    shader.display_name = "Skeletal Stylized"
    shader.description = "Fantasy-style shader with skeletal animation support"
    shader.is_skeletal = True
    return shader


def _create_transparent_shader() -> ShaderDefinition:
    """Create Transparent/Glass shader with refraction and fresnel effects"""
    all_backends = {
        GraphicsBackend.OPENGL, GraphicsBackend.WEBGL, GraphicsBackend.VULKAN,
        GraphicsBackend.DIRECTX11, GraphicsBackend.DIRECTX12, GraphicsBackend.METAL
    }
    return ShaderDefinition(
        name="Transparent",
        display_name="Transparent",
        description="Glass/transparent shader with refraction and fresnel effects",
        supported_backends=all_backends,
        categories=[
            ShaderCategory(
                name="Albedo",
                display_name="Color / Tint",
                parameters=[
                    ShaderParameter("albedoColor", "Color", ParameterType.COLOR,
                                  default_value=(1.0, 1.0, 1.0, 1.0),
                                  description="Base color tint for the glass"),
                    ShaderParameter("albedoTexture", "Texture", ParameterType.TEXTURE,
                                  description="Optional color/pattern texture"),
                ]
            ),
            ShaderCategory(
                name="TransparencySettings",
                display_name="Transparency",
                parameters=[
                    # u_TransparentParams: x=opacity, y=refractiveIndex, z=chromaticAberration, w=fresnelPower
                    ShaderParameter("transparentOpacity", "Opacity", ParameterType.FLOAT,
                                  default_value=0.5, min_value=0.0, max_value=1.0,
                                  description="Base opacity (0 = fully transparent, 1 = opaque)",
                                  uniform_name="u_TransparentParams", uniform_component=0),
                    ShaderParameter("transparentRefractiveIndex", "Refractive Index", ParameterType.FLOAT,
                                  default_value=1.5, min_value=1.0, max_value=3.0,
                                  description="Index of refraction (1.0 = air, 1.5 = glass, 2.4 = diamond)",
                                  uniform_name="u_TransparentParams", uniform_component=1),
                    ShaderParameter("transparentChromaticAberration", "Chromatic Aberration", ParameterType.FLOAT,
                                  default_value=0.0, min_value=0.0, max_value=0.5,
                                  description="Color separation amount (0 = none)",
                                  uniform_name="u_TransparentParams", uniform_component=2),
                    # u_TransparentParams2: x=reflectivity, y=roughness, z=thickness, w=normalScale
                    ShaderParameter("transparentThickness", "Thickness", ParameterType.FLOAT,
                                  default_value=1.0, min_value=0.1, max_value=5.0,
                                  description="Glass thickness (affects refraction offset)",
                                  uniform_name="u_TransparentParams2", uniform_component=2),
                ]
            ),
            ShaderCategory(
                name="FresnelSettings",
                display_name="Fresnel / Reflection",
                parameters=[
                    ShaderParameter("transparentFresnelPower", "Fresnel Power", ParameterType.FLOAT,
                                  default_value=5.0, min_value=1.0, max_value=10.0,
                                  description="Fresnel effect strength at grazing angles",
                                  uniform_name="u_TransparentParams", uniform_component=3),
                    ShaderParameter("transparentReflectivity", "Reflectivity", ParameterType.FLOAT,
                                  default_value=0.5, min_value=0.0, max_value=1.0,
                                  description="How much environment is reflected",
                                  uniform_name="u_TransparentParams2", uniform_component=0),
                    ShaderParameter("transparentRoughness", "Roughness", ParameterType.FLOAT,
                                  default_value=0.0, min_value=0.0, max_value=1.0,
                                  description="Surface roughness (0 = smooth glass)",
                                  uniform_name="u_TransparentParams2", uniform_component=1),
                ]
            ),
            ShaderCategory(
                name="Normal",
                display_name="Normal",
                parameters=[
                    ShaderParameter("normalTexture", "Texture", ParameterType.TEXTURE,
                                  description="Normal map for surface detail"),
                    # u_TransparentParams2.w = normalScale
                    ShaderParameter("normalScale", "Scale", ParameterType.FLOAT,
                                  default_value=1.0, min_value=0.0, max_value=2.0,
                                  uniform_name="u_TransparentParams2", uniform_component=3),
                ]
            ),
            ShaderCategory(
                name="Emissive",
                display_name="Emissive",
                parameters=[
                    ShaderParameter("emissiveColor", "Color", ParameterType.COLOR,
                                  default_value=(0.0, 0.0, 0.0, 1.0)),
                    ShaderParameter("emissiveTexture", "Texture", ParameterType.TEXTURE),
                    # u_TransparentParams3.x = emissiveStrength
                    ShaderParameter("emissiveStrength", "Strength", ParameterType.FLOAT,
                                  default_value=1.0, min_value=0.0, max_value=10.0,
                                  uniform_name="u_TransparentParams3", uniform_component=0),
                ]
            ),
            ShaderCategory(
                name="Alpha",
                display_name="Alpha",
                parameters=[
                    # u_TransparentParams3.y = alphaCutoff
                    ShaderParameter("alphaCutoff", "Cutoff", ParameterType.FLOAT,
                                  default_value=0.0, min_value=0.0, max_value=1.0,
                                  description="Alpha cutoff threshold for discarding pixels",
                                  uniform_name="u_TransparentParams3", uniform_component=1),
                ]
            ),
        ]
    )


def _create_glow_shader() -> ShaderDefinition:
    """Create Glow/Emissive shader for stars, lights, and magic effects"""
    all_backends = {
        GraphicsBackend.OPENGL, GraphicsBackend.WEBGL, GraphicsBackend.VULKAN,
        GraphicsBackend.DIRECTX11, GraphicsBackend.DIRECTX12, GraphicsBackend.METAL
    }
    return ShaderDefinition(
        name="Glow",
        display_name="Glow",
        description="Emissive glow shader for stars, lights, particles, and magic effects",
        supported_backends=all_backends,
        categories=[
            ShaderCategory(
                name="Albedo",
                display_name="Color",
                parameters=[
                    ShaderParameter("albedoColor", "Color", ParameterType.COLOR,
                                  default_value=(1.0, 1.0, 1.0, 1.0),
                                  description="Base glow color"),
                    ShaderParameter("albedoTexture", "Texture", ParameterType.TEXTURE,
                                  description="Optional glow pattern texture"),
                ]
            ),
            ShaderCategory(
                name="GlowSettings",
                display_name="Glow Settings",
                parameters=[
                    # u_GlowParams: x=intensity, y=falloff, z=pulseSpeed, w=pulseAmount
                    ShaderParameter("glowIntensity", "Intensity", ParameterType.FLOAT,
                                  default_value=1.0, min_value=0.0, max_value=10.0,
                                  description="Overall glow brightness",
                                  uniform_name="u_GlowParams", uniform_component=0),
                    ShaderParameter("glowFalloff", "Falloff", ParameterType.FLOAT,
                                  default_value=2.0, min_value=0.5, max_value=10.0,
                                  description="How quickly glow fades from center",
                                  uniform_name="u_GlowParams", uniform_component=1),
                    # u_GlowParams2: x=coreSize, y=coreBrightness, z=outerGlow, w=alphaCutoff
                    ShaderParameter("glowCoreSize", "Core Size", ParameterType.FLOAT,
                                  default_value=0.3, min_value=0.0, max_value=1.0,
                                  description="Size of the bright core (0-1)",
                                  uniform_name="u_GlowParams2", uniform_component=0),
                    ShaderParameter("glowCoreBrightness", "Core Brightness", ParameterType.FLOAT,
                                  default_value=2.0, min_value=0.0, max_value=10.0,
                                  description="Brightness multiplier for the core",
                                  uniform_name="u_GlowParams2", uniform_component=1),
                    ShaderParameter("glowOuterGlow", "Outer Glow", ParameterType.FLOAT,
                                  default_value=1.0, min_value=0.0, max_value=5.0,
                                  description="Outer glow intensity",
                                  uniform_name="u_GlowParams2", uniform_component=2),
                ]
            ),
            ShaderCategory(
                name="Animation",
                display_name="Animation",
                parameters=[
                    ShaderParameter("glowPulseSpeed", "Pulse Speed", ParameterType.FLOAT,
                                  default_value=0.0, min_value=0.0, max_value=10.0,
                                  description="Animation speed (0 = no pulse)",
                                  uniform_name="u_GlowParams", uniform_component=2),
                    ShaderParameter("glowPulseAmount", "Pulse Amount", ParameterType.FLOAT,
                                  default_value=0.0, min_value=0.0, max_value=1.0,
                                  description="Pulse intensity variation",
                                  uniform_name="u_GlowParams", uniform_component=3),
                ]
            ),
            ShaderCategory(
                name="RimGlow",
                display_name="Rim Glow",
                parameters=[
                    # u_GlowParams3: x=fresnelPower, y=fresnelIntensity, z=time, w=colorShift
                    ShaderParameter("glowFresnelPower", "Fresnel Power", ParameterType.FLOAT,
                                  default_value=3.0, min_value=0.5, max_value=10.0,
                                  description="Rim glow effect power",
                                  uniform_name="u_GlowParams3", uniform_component=0),
                    ShaderParameter("glowFresnelIntensity", "Fresnel Intensity", ParameterType.FLOAT,
                                  default_value=0.5, min_value=0.0, max_value=2.0,
                                  description="Rim glow intensity",
                                  uniform_name="u_GlowParams3", uniform_component=1),
                ]
            ),
            ShaderCategory(
                name="ColorEffect",
                display_name="Color Effect",
                parameters=[
                    ShaderParameter("glowColorShift", "Color Shift", ParameterType.FLOAT,
                                  default_value=0.0, min_value=-1.0, max_value=1.0,
                                  description="Color temperature shift (-1 = cool/blue, +1 = warm/orange)",
                                  uniform_name="u_GlowParams3", uniform_component=3),
                ]
            ),
            ShaderCategory(
                name="Emissive",
                display_name="Emissive Texture",
                parameters=[
                    ShaderParameter("emissiveColor", "Color", ParameterType.COLOR,
                                  default_value=(1.0, 1.0, 1.0, 1.0),
                                  description="Emissive color multiplier"),
                    ShaderParameter("emissiveTexture", "Texture", ParameterType.TEXTURE,
                                  description="Emissive pattern texture"),
                ]
            ),
            ShaderCategory(
                name="Alpha",
                display_name="Alpha",
                parameters=[
                    # u_GlowParams2.w = alphaCutoff
                    ShaderParameter("alphaCutoff", "Cutoff", ParameterType.FLOAT,
                                  default_value=0.0, min_value=0.0, max_value=1.0,
                                  description="Alpha cutoff threshold",
                                  uniform_name="u_GlowParams2", uniform_component=3),
                ]
            ),
        ]
    )


def _create_custom_shader() -> ShaderDefinition:
    """Create placeholder for custom shader - parameters loaded dynamically"""
    # Custom shaders can potentially support any backend - user provides paths per-backend
    all_backends = {
        GraphicsBackend.OPENGL, GraphicsBackend.WEBGL, GraphicsBackend.VULKAN,
        GraphicsBackend.DIRECTX11, GraphicsBackend.DIRECTX12, GraphicsBackend.METAL
    }
    return ShaderDefinition(
        name="Custom",
        display_name="Custom",
        description="Custom shader - specify vertex and fragment shader paths for your backend",
        supported_backends=all_backends,
        is_custom=True,
        categories=[]  # Parameters added dynamically based on shader introspection
    )


# ============================================================================
# Shader Registry
# ============================================================================

class ShaderRegistry:
    """
    Registry of available shaders and their definitions.

    Provides access to built-in shader definitions and manages custom shaders.
    Custom shaders can be registered at runtime.

    Backend Filtering:
    - Use set_current_backend() to set the active graphics backend
    - Methods with _for_backend suffix return only shaders supporting that backend
    - Default methods use the current backend if set
    """

    _instance = None

    def __new__(cls):
        if cls._instance is None:
            cls._instance = super().__new__(cls)
            cls._instance._initialized = False
        return cls._instance

    def __init__(self):
        if self._initialized:
            return
        self._initialized = True

        self._shaders: Dict[str, ShaderDefinition] = {}
        self._custom_shaders: Dict[str, ShaderDefinition] = {}
        self._current_backend: GraphicsBackend = GraphicsBackend.OPENGL

        # Register built-in shaders
        self._register_builtin_shaders()

    def _register_builtin_shaders(self):
        """Register all built-in shaders"""
        builtins = [
            _create_pbr_shader(),
            _create_skeletal_shader(),
            _create_toon_shader(),
            _create_skeletal_toon_shader(),
            _create_stylized_shader(),
            _create_skeletal_stylized_shader(),
            _create_transparent_shader(),
            _create_glow_shader(),
            _create_unlit_shader(),
            _create_standard3d_shader(),
            _create_custom_shader(),
        ]
        for shader in builtins:
            self._shaders[shader.name] = shader

    def set_current_backend(self, backend: GraphicsBackend):
        """Set the current graphics backend for filtering"""
        self._current_backend = backend

    def set_current_backend_from_engine(self, engine_backend):
        """Set the current backend from an engine GraphicsBackend enum"""
        self._current_backend = GraphicsBackend.from_engine(engine_backend)

    def get_current_backend(self) -> GraphicsBackend:
        """Get the current graphics backend"""
        return self._current_backend

    def get_shader(self, name: str) -> Optional[ShaderDefinition]:
        """Get a shader definition by name"""
        if name in self._shaders:
            return self._shaders[name]
        return self._custom_shaders.get(name)

    def get_builtin_shaders(self, backend: GraphicsBackend = None) -> List[ShaderDefinition]:
        """Get list of built-in shader definitions for the specified backend"""
        backend = backend or self._current_backend
        return [s for s in self._shaders.values()
                if not s.is_custom and s.supports_backend(backend)]

    def get_builtin_shader_names(self, backend: GraphicsBackend = None) -> List[str]:
        """Get list of built-in shader display names for the specified backend"""
        backend = backend or self._current_backend
        return [s.display_name for s in self._shaders.values()
                if not s.is_custom and s.supports_backend(backend)]

    def get_all_shader_names(self, backend: GraphicsBackend = None) -> List[str]:
        """Get list of all shader names (built-in + custom) for the specified backend"""
        backend = backend or self._current_backend
        names = [s.display_name for s in self._shaders.values()
                 if s.supports_backend(backend)]
        names.extend([s.display_name for s in self._custom_shaders.values()
                      if s.supports_backend(backend)])
        return names

    def register_custom_shader(self, shader: ShaderDefinition, backend: GraphicsBackend = None):
        """Register a custom shader definition"""
        shader.is_custom = True
        if backend:
            shader.supported_backends = {backend}
        self._custom_shaders[shader.name] = shader

    def unregister_custom_shader(self, name: str):
        """Remove a custom shader from the registry"""
        if name in self._custom_shaders:
            del self._custom_shaders[name]

    def get_shader_by_display_name(self, display_name: str,
                                    backend: GraphicsBackend = None) -> Optional[ShaderDefinition]:
        """Get shader by its display name, optionally filtered by backend"""
        backend = backend or self._current_backend
        for shader in self._shaders.values():
            if shader.display_name == display_name and shader.supports_backend(backend):
                return shader
        for shader in self._custom_shaders.values():
            if shader.display_name == display_name and shader.supports_backend(backend):
                return shader
        return None

    def get_backend_display_name(self, backend: GraphicsBackend = None) -> str:
        """Get a human-readable name for the backend"""
        backend = backend or self._current_backend
        return backend.value


def get_shader_registry() -> ShaderRegistry:
    """Get the global shader registry instance"""
    return ShaderRegistry()


# ============================================================================
# Custom Shader Parameter Parsing (Architecture for future implementation)
# ============================================================================

class CustomShaderLoader:
    """
    Loader for custom shader parameter definitions.

    Future implementation will:
    1. Parse shader source files for uniform declarations
    2. Load companion .shader_meta.json files for parameter metadata
    3. Use reflection/introspection to determine parameter types

    For now, provides a stub implementation and the architecture for future use.
    """

    @staticmethod
    def load_from_file(shader_path: str) -> Optional[ShaderDefinition]:
        """
        Load a custom shader definition from a shader file.

        Args:
            shader_path: Path to the vertex or fragment shader file

        Returns:
            ShaderDefinition with parsed parameters, or None if failed
        """
        # TODO: Implement shader parsing
        # For now, return a basic custom shader with common PBR params
        # Set the shader path for common backends (single-file shaders work across backends)
        return ShaderDefinition(
            name=f"Custom_{shader_path}",
            display_name="Custom Shader",
            description=f"Custom shader from {shader_path}",
            is_custom=True,
            custom_paths={
                GraphicsBackend.OPENGL: shader_path,
                GraphicsBackend.VULKAN: shader_path,
                GraphicsBackend.DIRECTX11: shader_path,
                GraphicsBackend.DIRECTX12: shader_path,
            },
            categories=[
                # Default to PBR-like categories for now
                ShaderCategory(
                    name="Albedo",
                    display_name="Albedo",
                    parameters=[
                        ShaderParameter("albedoColor", "Color", ParameterType.COLOR,
                                      default_value=(1.0, 1.0, 1.0, 1.0)),
                        ShaderParameter("albedoTexture", "Texture", ParameterType.TEXTURE),
                    ]
                ),
            ]
        )

    @staticmethod
    def load_from_metadata(meta_path: str) -> Optional[ShaderDefinition]:
        """
        Load shader definition from a metadata JSON file.

        Expected format:
        {
            "name": "MyShader",
            "display_name": "My Custom Shader",
            "description": "A custom shader",
            "vertex_shader": "path/to/vertex.vert",
            "fragment_shader": "path/to/fragment.frag",
            "categories": [
                {
                    "name": "Category1",
                    "display_name": "Category 1",
                    "parameters": [
                        {
                            "name": "param1",
                            "display_name": "Parameter 1",
                            "type": "float",
                            "default": 1.0,
                            "min": 0.0,
                            "max": 2.0
                        }
                    ]
                }
            ]
        }
        """
        import json
        import os

        if not os.path.exists(meta_path):
            return None

        try:
            with open(meta_path, 'r') as f:
                data = json.load(f)

            categories = []
            for cat_data in data.get('categories', []):
                params = []
                for param_data in cat_data.get('parameters', []):
                    param_type = ParameterType(param_data.get('type', 'float'))
                    params.append(ShaderParameter(
                        name=param_data['name'],
                        display_name=param_data.get('display_name', param_data['name']),
                        param_type=param_type,
                        default_value=param_data.get('default'),
                        min_value=param_data.get('min'),
                        max_value=param_data.get('max'),
                        description=param_data.get('description', '')
                    ))
                categories.append(ShaderCategory(
                    name=cat_data['name'],
                    display_name=cat_data.get('display_name', cat_data['name']),
                    parameters=params
                ))

            return ShaderDefinition(
                name=data['name'],
                display_name=data.get('display_name', data['name']),
                description=data.get('description', ''),
                is_custom=True,
                custom_path=data.get('vertex_shader', ''),
                categories=categories
            )

        except Exception as e:
            print(f"[Warning] Failed to load shader metadata from {meta_path}: {e}")
            return None


# ============================================================================
# Lupine Shader (.lsh) Parameter Introspection
# ============================================================================
#
# Parses the `#begin properties ... #end properties` block of a .lsh file and
# extracts the "exported" parameters: uniforms that are NOT provided automatically
# by the engine. These are the parameters a user can edit per-component (e.g. on a
# ColorRect). Optional `@annotations` provide nicer editing metadata and are written
# in the quoted form the .lsh transpiler already strips, e.g.:
#
#   uniform float u_GlowStrength = 1.0; @display "Glow Strength" @range "0.0,5.0"
#   uniform vec4  u_GlowColor = vec4(1.0); @display "Glow Color" @color "true"

# Uniforms supplied by the engine for 2D-UI custom shaders (ColorRect, Image2D, Panel,
# Shape2D). These are filled automatically at render time and are never shown as editable
# parameters in the inspector.
UI_SYSTEM_UNIFORMS = {
    # Camera / transform (engine-bound)
    "u_ViewProjection", "u_Model", "u_View", "u_Projection", "u_NormalMatrix",
    "u_CameraPosition", "u_Time",
    # Per-frame screen-space helpers fed by the renderer (Gap E/F)
    "u_TexelSize", "u_Resolution",
    # Standard rounded-rect / image uniforms
    "u_TintColor", "u_CornerRadius", "u_Size", "u_UVRect", "u_UseTexture", "u_Texture",
    # Shape2D shape description uniforms
    "u_ShapeType", "u_ShapeParams", "u_BorderColor", "u_BorderParams",
    # Scene-texture post-processing (engine-bound)
    "u_SceneTexture", "u_SceneFlipY", "u_ScreenSize",
}

# Backward-compatible alias.
COLORRECT_SYSTEM_UNIFORMS = UI_SYSTEM_UNIFORMS

_GLSL_TYPE_TO_PARAM = {
    "float": ParameterType.FLOAT,
    "int": ParameterType.INT,
    "bool": ParameterType.BOOL,
    "vec2": ParameterType.VEC2,
    "vec3": ParameterType.VEC3,
    "vec4": ParameterType.VEC4,
    "sampler2D": ParameterType.TEXTURE,
    "samplerCube": ParameterType.TEXTURE,
}

_LSH_UNIFORM_RE = re.compile(
    r"^uniform\s+(\w+)\s+(\w+)\s*(?:\[(\d+)\])?\s*(?:=\s*(.+?))?\s*;?\s*$"
)
_LSH_ANNOTATION_RE = re.compile(r'@(\w+)\s+"([^"]*)"')


def _prettify_uniform_name(name: str) -> str:
    """Turn a uniform name like 'u_GlowStrength' into a display label 'Glow Strength'."""
    base = name[2:] if name.startswith("u_") else name
    if not base:
        return name
    # Split camelCase / PascalCase and underscores into words.
    spaced = re.sub(r'(?<=[a-z0-9])(?=[A-Z])', ' ', base)
    spaced = spaced.replace('_', ' ')
    return ' '.join(word.capitalize() for word in spaced.split())


def _strip_ctor(text: str) -> str:
    """Strip a scalar constructor wrapper, e.g. 'float(1.0)' -> '1.0'."""
    match = re.match(r'^\s*\w+\s*\(\s*(.+?)\s*\)\s*$', text)
    return match.group(1) if match else text


def _parse_vector_components(text: str, count: int):
    """Parse a 'vecN(...)' (or comma list) expression into `count` float components."""
    inner = text.strip()
    ctor = re.match(r'^\s*vec[234]\s*\(\s*(.*?)\s*\)\s*$', inner)
    if ctor:
        inner = ctor.group(1)
    parts = [p.strip() for p in inner.split(',') if p.strip()]
    if not parts:
        return None
    try:
        values = [float(p) for p in parts]
    except ValueError:
        return None
    if len(values) == 1:
        values = values * count
    if len(values) < count:
        values = values + [values[-1]] * (count - len(values))
    return values[:count]


def _parse_lsh_default(glsl_type: str, raw_default: Optional[str]):
    """Parse a GLSL default-value expression into a Python value for the editor."""
    if raw_default is None:
        return None
    text = raw_default.strip().rstrip(';').strip()
    if not text:
        return None
    try:
        if glsl_type == "float":
            return float(_strip_ctor(text))
        if glsl_type == "int":
            return int(float(_strip_ctor(text)))
        if glsl_type == "bool":
            return text.lower() == "true"
        if glsl_type in ("vec2", "vec3", "vec4"):
            count = int(glsl_type[3])
            comps = _parse_vector_components(text, count)
            return tuple(comps) if comps else None
    except (ValueError, TypeError):
        return None
    return None


def _extract_properties_block(source: str) -> str:
    """Return the text between '#begin properties' and '#end properties' (or '')."""
    lines = source.splitlines()
    collecting = False
    block_lines = []
    for line in lines:
        stripped = line.strip()
        if stripped.startswith('#begin') and 'properties' in stripped.split():
            collecting = True
            continue
        if stripped.startswith('#end') and 'properties' in stripped.split():
            break
        if collecting:
            block_lines.append(line)
    return '\n'.join(block_lines)


def parse_lsh_parameters(source: str,
                         system_uniforms: Set[str] = None) -> List[ShaderParameter]:
    """
    Parse the exported parameters from .lsh source.

    Args:
        source: The full .lsh file contents.
        system_uniforms: Uniform names to treat as engine-provided (excluded from the
                         result). Defaults to COLORRECT_SYSTEM_UNIFORMS.

    Returns:
        A list of ShaderParameter describing each editable uniform, in declaration order.
    """
    if system_uniforms is None:
        system_uniforms = COLORRECT_SYSTEM_UNIFORMS

    block = _extract_properties_block(source)
    if not block:
        return []

    params: List[ShaderParameter] = []
    for raw_line in block.splitlines():
        line = raw_line.strip()
        if not line or line.startswith('//'):
            continue

        # Pull annotations out before stripping comments and matching the declaration.
        annotations = {key: val for key, val in _LSH_ANNOTATION_RE.findall(line)}
        cleaned = _LSH_ANNOTATION_RE.sub('', line)
        cleaned = re.sub(r'//.*$', '', cleaned).strip()

        match = _LSH_UNIFORM_RE.match(cleaned)
        if not match:
            continue

        glsl_type, name, array_size, raw_default = match.groups()
        if name in system_uniforms:
            continue
        if glsl_type not in _GLSL_TYPE_TO_PARAM:
            # Skip engine-managed/unsupported types (mat3, mat4, etc.).
            continue

        param_type = _GLSL_TYPE_TO_PARAM[glsl_type]

        # A vec4 may be flagged as a color via @color "true".
        if param_type == ParameterType.VEC4 and annotations.get('color', '').lower() in ('true', '1', 'yes'):
            param_type = ParameterType.COLOR

        default_value = _parse_lsh_default(glsl_type, raw_default)
        if param_type == ParameterType.COLOR and default_value is None:
            default_value = (1.0, 1.0, 1.0, 1.0)

        min_value = None
        max_value = None
        if 'range' in annotations:
            range_parts = [p.strip() for p in annotations['range'].split(',')]
            if len(range_parts) == 2:
                try:
                    min_value = float(range_parts[0])
                    max_value = float(range_parts[1])
                except ValueError:
                    min_value = None
                    max_value = None

        display_name = annotations.get('display') or _prettify_uniform_name(name)
        description = annotations.get('description', '')

        params.append(ShaderParameter(
            name=name,
            display_name=display_name,
            param_type=param_type,
            default_value=default_value,
            min_value=min_value,
            max_value=max_value,
            description=description,
        ))

    return params


def parse_lsh_metadata(source: str) -> Tuple[str, str]:
    """Extract the (name, description) declared by '#shader' / '#description' directives."""
    name = ""
    description = ""
    name_match = re.search(r'#shader\s+"([^"]*)"', source)
    if name_match:
        name = name_match.group(1)
    desc_match = re.search(r'#description\s+"([^"]*)"', source)
    if desc_match:
        description = desc_match.group(1)
    return name, description


def parse_lsh_file(path: str,
                   system_uniforms: Set[str] = None) -> List[ShaderParameter]:
    """Read a .lsh file from disk and return its exported parameters (empty on failure)."""
    if not path or not os.path.exists(path):
        return []
    try:
        with open(path, 'r', encoding='utf-8') as f:
            source = f.read()
    except OSError:
        return []
    return parse_lsh_parameters(source, system_uniforms)
