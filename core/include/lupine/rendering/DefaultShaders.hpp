#pragma once

#include "GeneratedShaders.hpp"
#include "ShaderTranslator.hpp"
#include "gfx/GfxTypes.hpp"
#include <cstring>

namespace lupine {
namespace DefaultShaders {

/**
 * Backend-agnostic shader access
 * Provides shader sources that automatically select the appropriate backend
 * based on the current GraphicsBackend in use.
 *
 * All shaders are accessed as functions for uniform API regardless of size.
 */

// Shader type helper
struct ShaderPair {
    const char* vertex;
    const char* fragment;
};

// Precompiled shader bytecode (for backends that support offline compilation)
struct ShaderBytecode {
    const uint8_t* data = nullptr;
    size_t size = 0;

    bool isValid() const { return data != nullptr && size > 0; }
};

// Shader bytecode pair for vertex and fragment shaders
struct ShaderBytecodePair {
    ShaderBytecode vertex;
    ShaderBytecode fragment;
    ShaderBytecode geometry;  // Optional, for shaders with geometry stage

    bool hasPrecompiledVertex() const { return vertex.isValid(); }
    bool hasPrecompiledFragment() const { return fragment.isValid(); }
    bool hasPrecompiledGeometry() const { return geometry.isValid(); }
};

/**
 * Get shader source for a specific shader and backend
 * @param shaderName Name of the shader (e.g., "Standard3D", "PBR", etc.)
 * @param backend Graphics backend to get shaders for
 * @param outVertex Pointer to receive vertex shader source
 * @param outFragment Pointer to receive fragment shader source
 * @return true if shader was found, false otherwise
 */
inline bool getShader(const char* shaderName, GraphicsBackend backend,
                      const char** outVertex, const char** outFragment) {
    using namespace GeneratedShaders;

    // Map shader name to backend-specific sources
    if (strcmp(shaderName, "Standard3D") == 0) {
        switch (backend) {
            case GraphicsBackend::OpenGL:
                *outVertex = OpenGL::standard3_d_vert();
                *outFragment = OpenGL::standard3_d_frag();
                return true;
            case GraphicsBackend::Vulkan:
                *outVertex = Vulkan::standard3_d_vert();
                *outFragment = Vulkan::standard3_d_frag();
                return true;
            case GraphicsBackend::DirectX11:
                *outVertex = DirectX11::standard3_d_vert();
                *outFragment = DirectX11::standard3_d_frag();
                return true;
            case GraphicsBackend::DirectX12:
                *outVertex = DirectX12::standard3_d_vert();
                *outFragment = DirectX12::standard3_d_frag();
                return true;
            case GraphicsBackend::Metal:
                *outVertex = Metal::standard3_d_metal();
                *outFragment = Metal::standard3_d_metal();
                return true;
            case GraphicsBackend::WebGL:
                *outVertex = WebGL::standard3_d_vert();
                *outFragment = WebGL::standard3_d_frag();
                return true;
            default:
                return false;
        }
    }
    else if (strcmp(shaderName, "PBR") == 0) {
        switch (backend) {
            case GraphicsBackend::OpenGL:
                *outVertex = OpenGL::pbr_vert();
                *outFragment = OpenGL::pbr_frag();
                return true;
            case GraphicsBackend::Vulkan:
                *outVertex = Vulkan::pbr_vert();
                *outFragment = Vulkan::pbr_frag();
                return true;
            case GraphicsBackend::DirectX11:
                *outVertex = DirectX11::pbr_vert();
                *outFragment = DirectX11::pbr_frag();
                return true;
            case GraphicsBackend::DirectX12:
                *outVertex = DirectX12::pbr_vert();
                *outFragment = DirectX12::pbr_frag();
                return true;
            case GraphicsBackend::Metal:
                *outVertex = Metal::pbr_metal();
                *outFragment = Metal::pbr_metal();
                return true;
            case GraphicsBackend::WebGL:
                *outVertex = WebGL::pbr_vert();
                *outFragment = WebGL::pbr_frag();
                return true;
            default:
                return false;
        }
    }
    else if (strcmp(shaderName, "PBRInstanced") == 0) {
        switch (backend) {
            case GraphicsBackend::OpenGL:
                *outVertex = OpenGL::pbr_instanced_vert();
                *outFragment = OpenGL::pbr_instanced_frag();
                return true;
            case GraphicsBackend::Vulkan:
                *outVertex = Vulkan::pbr_instanced_vert();
                *outFragment = Vulkan::pbr_instanced_frag();
                return true;
            case GraphicsBackend::DirectX11:
                *outVertex = DirectX11::pbr_instanced_vert();
                *outFragment = DirectX11::pbr_instanced_frag();
                return true;
            case GraphicsBackend::DirectX12:
                *outVertex = DirectX12::pbr_instanced_vert();
                *outFragment = DirectX12::pbr_instanced_frag();
                return true;
            case GraphicsBackend::Metal:
                *outVertex = Metal::pbr_instanced_metal();
                *outFragment = Metal::pbr_instanced_metal();
                return true;
            case GraphicsBackend::WebGL:
                *outVertex = WebGL::pbr_instanced_vert();
                *outFragment = WebGL::pbr_instanced_frag();
                return true;
            default:
                return false;
        }
    }
    else if (strcmp(shaderName, "ShadowMapInstanced") == 0) {
        switch (backend) {
            case GraphicsBackend::OpenGL:
                *outVertex = OpenGL::shadow_map_instanced_vert();
                *outFragment = OpenGL::shadow_map_instanced_frag();
                return true;
            case GraphicsBackend::Vulkan:
                *outVertex = Vulkan::shadow_map_instanced_vert();
                *outFragment = Vulkan::shadow_map_instanced_frag();
                return true;
            case GraphicsBackend::DirectX11:
                *outVertex = DirectX11::shadow_map_instanced_vert();
                *outFragment = DirectX11::shadow_map_instanced_frag();
                return true;
            case GraphicsBackend::DirectX12:
                *outVertex = DirectX12::shadow_map_instanced_vert();
                *outFragment = DirectX12::shadow_map_instanced_frag();
                return true;
            case GraphicsBackend::Metal:
                *outVertex = Metal::shadow_map_instanced_metal();
                *outFragment = Metal::shadow_map_instanced_metal();
                return true;
            case GraphicsBackend::WebGL:
                *outVertex = WebGL::shadow_map_instanced_vert();
                *outFragment = WebGL::shadow_map_instanced_frag();
                return true;
            default:
                return false;
        }
    }
    else if (strcmp(shaderName, "ShadowCubeInstanced") == 0) {
        switch (backend) {
            case GraphicsBackend::OpenGL:
                *outVertex = OpenGL::shadow_cube_instanced_vert();
                *outFragment = OpenGL::shadow_cube_instanced_frag();
                return true;
            case GraphicsBackend::Vulkan:
                *outVertex = Vulkan::shadow_cube_instanced_vert();
                *outFragment = Vulkan::shadow_cube_instanced_frag();
                return true;
            case GraphicsBackend::DirectX11:
                *outVertex = DirectX11::shadow_cube_instanced_vert();
                *outFragment = DirectX11::shadow_cube_instanced_frag();
                return true;
            case GraphicsBackend::DirectX12:
                *outVertex = DirectX12::shadow_cube_instanced_vert();
                *outFragment = DirectX12::shadow_cube_instanced_frag();
                return true;
            case GraphicsBackend::Metal:
                *outVertex = Metal::shadow_cube_instanced_metal();
                *outFragment = Metal::shadow_cube_instanced_metal();
                return true;
            case GraphicsBackend::WebGL:
                *outVertex = WebGL::shadow_cube_instanced_vert();
                *outFragment = WebGL::shadow_cube_instanced_frag();
                return true;
            default:
                return false;
        }
    }
    else if (strcmp(shaderName, "Skeletal") == 0) {
        switch (backend) {
            case GraphicsBackend::OpenGL:
                *outVertex = OpenGL::skeletal_vert();
                *outFragment = OpenGL::skeletal_frag();
                return true;
            case GraphicsBackend::Vulkan:
                *outVertex = Vulkan::skeletal_vert();
                *outFragment = Vulkan::skeletal_frag();
                return true;
            case GraphicsBackend::DirectX11:
                *outVertex = DirectX11::skeletal_vert();
                *outFragment = DirectX11::skeletal_frag();
                return true;
            case GraphicsBackend::DirectX12:
                *outVertex = DirectX12::skeletal_vert();
                *outFragment = DirectX12::skeletal_frag();
                return true;
            case GraphicsBackend::Metal:
                *outVertex = Metal::skeletal_metal();
                *outFragment = Metal::skeletal_metal();
                return true;
            case GraphicsBackend::WebGL:
                *outVertex = WebGL::skeletal_vert();
                *outFragment = WebGL::skeletal_frag();
                return true;
            default:
                return false;
        }
    }
    else if (strcmp(shaderName, "Sprite2D") == 0) {
        switch (backend) {
            case GraphicsBackend::OpenGL:
                *outVertex = OpenGL::sprite2_d_vert();
                *outFragment = OpenGL::sprite2_d_frag();
                return true;
            case GraphicsBackend::Vulkan:
                *outVertex = Vulkan::sprite2_d_vert();
                *outFragment = Vulkan::sprite2_d_frag();
                return true;
            case GraphicsBackend::DirectX11:
                *outVertex = DirectX11::sprite2_d_vert();
                *outFragment = DirectX11::sprite2_d_frag();
                return true;
            case GraphicsBackend::DirectX12:
                *outVertex = DirectX12::sprite2_d_vert();
                *outFragment = DirectX12::sprite2_d_frag();
                return true;
            case GraphicsBackend::Metal:
                *outVertex = Metal::sprite2_d_metal();
                *outFragment = Metal::sprite2_d_metal();
                return true;
            case GraphicsBackend::WebGL:
                *outVertex = WebGL::sprite2_d_vert();
                *outFragment = WebGL::sprite2_d_frag();
                return true;
            default:
                return false;
        }
    }
    else if (strcmp(shaderName, "Sprite2D_AlphaCutoff") == 0) {
        switch (backend) {
            case GraphicsBackend::OpenGL:
                *outVertex = OpenGL::sprite2_d_alpha_cutoff_vert();
                *outFragment = OpenGL::sprite2_d_alpha_cutoff_frag();
                return true;
            case GraphicsBackend::Vulkan:
                *outVertex = Vulkan::sprite2_d_alpha_cutoff_vert();
                *outFragment = Vulkan::sprite2_d_alpha_cutoff_frag();
                return true;
            case GraphicsBackend::DirectX11:
                *outVertex = DirectX11::sprite2_d_alpha_cutoff_vert();
                *outFragment = DirectX11::sprite2_d_alpha_cutoff_frag();
                return true;
            case GraphicsBackend::DirectX12:
                *outVertex = DirectX12::sprite2_d_alpha_cutoff_vert();
                *outFragment = DirectX12::sprite2_d_alpha_cutoff_frag();
                return true;
            case GraphicsBackend::Metal:
                *outVertex = Metal::sprite2_d_alpha_cutoff_metal();
                *outFragment = Metal::sprite2_d_alpha_cutoff_metal();
                return true;
            case GraphicsBackend::WebGL:
                *outVertex = WebGL::sprite2_d_alpha_cutoff_vert();
                *outFragment = WebGL::sprite2_d_alpha_cutoff_frag();
                return true;
            default:
                return false;
        }
    }
    else if (strcmp(shaderName, "Sprite3D") == 0) {
        switch (backend) {
            case GraphicsBackend::OpenGL:
                *outVertex = OpenGL::sprite3_d_vert();
                *outFragment = OpenGL::sprite3_d_frag();
                return true;
            case GraphicsBackend::Vulkan:
                *outVertex = Vulkan::sprite3_d_vert();
                *outFragment = Vulkan::sprite3_d_frag();
                return true;
            case GraphicsBackend::DirectX11:
                *outVertex = DirectX11::sprite3_d_vert();
                *outFragment = DirectX11::sprite3_d_frag();
                return true;
            case GraphicsBackend::DirectX12:
                *outVertex = DirectX12::sprite3_d_vert();
                *outFragment = DirectX12::sprite3_d_frag();
                return true;
            case GraphicsBackend::Metal:
                *outVertex = Metal::sprite3_d_metal();
                *outFragment = Metal::sprite3_d_metal();
                return true;
            case GraphicsBackend::WebGL:
                *outVertex = WebGL::sprite3_d_vert();
                *outFragment = WebGL::sprite3_d_frag();
                return true;
            default:
                return false;
        }
    }
    else if (strcmp(shaderName, "Text") == 0) {
        switch (backend) {
            case GraphicsBackend::OpenGL:
                *outVertex = OpenGL::text_vert();
                *outFragment = OpenGL::text_frag();
                return true;
            case GraphicsBackend::Vulkan:
                *outVertex = Vulkan::text_vert();
                *outFragment = Vulkan::text_frag();
                return true;
            case GraphicsBackend::DirectX11:
                *outVertex = DirectX11::text_vert();
                *outFragment = DirectX11::text_frag();
                return true;
            case GraphicsBackend::DirectX12:
                *outVertex = DirectX12::text_vert();
                *outFragment = DirectX12::text_frag();
                return true;
            case GraphicsBackend::Metal:
                *outVertex = Metal::text_metal();
                *outFragment = Metal::text_metal();
                return true;
            case GraphicsBackend::WebGL:
                *outVertex = WebGL::text_vert();
                *outFragment = WebGL::text_frag();
                return true;
            default:
                return false;
        }
    }
    else if (strcmp(shaderName, "Text3D") == 0) {
        switch (backend) {
            case GraphicsBackend::OpenGL:
                *outVertex = OpenGL::text3_d_vert();
                *outFragment = OpenGL::text3_d_frag();
                return true;
            case GraphicsBackend::Vulkan:
                *outVertex = Vulkan::text3_d_vert();
                *outFragment = Vulkan::text3_d_frag();
                return true;
            case GraphicsBackend::DirectX11:
                *outVertex = DirectX11::text3_d_vert();
                *outFragment = DirectX11::text3_d_frag();
                return true;
            case GraphicsBackend::DirectX12:
                *outVertex = DirectX12::text3_d_vert();
                *outFragment = DirectX12::text3_d_frag();
                return true;
            case GraphicsBackend::Metal:
                *outVertex = Metal::text3_d_metal();
                *outFragment = Metal::text3_d_metal();
                return true;
            case GraphicsBackend::WebGL:
                *outVertex = WebGL::text3_d_vert();
                *outFragment = WebGL::text3_d_frag();
                return true;
            default:
                return false;
        }
    }
    else if (strcmp(shaderName, "Unlit") == 0) {
        switch (backend) {
            case GraphicsBackend::OpenGL:
                *outVertex = OpenGL::unlit_vert();
                *outFragment = OpenGL::unlit_frag();
                return true;
            case GraphicsBackend::Vulkan:
                *outVertex = Vulkan::unlit_vert();
                *outFragment = Vulkan::unlit_frag();
                return true;
            case GraphicsBackend::DirectX11:
                *outVertex = DirectX11::unlit_vert();
                *outFragment = DirectX11::unlit_frag();
                return true;
            case GraphicsBackend::DirectX12:
                *outVertex = DirectX12::unlit_vert();
                *outFragment = DirectX12::unlit_frag();
                return true;
            case GraphicsBackend::Metal:
                *outVertex = Metal::unlit_metal();
                *outFragment = Metal::unlit_metal();
                return true;
            case GraphicsBackend::WebGL:
                *outVertex = WebGL::unlit_vert();
                *outFragment = WebGL::unlit_frag();
                return true;
            default:
                return false;
        }
    }
    else if (strcmp(shaderName, "Wireframe") == 0) {
        switch (backend) {
            case GraphicsBackend::OpenGL:
                *outVertex = OpenGL::wireframe_vert();
                *outFragment = OpenGL::wireframe_frag();
                return true;
            case GraphicsBackend::Vulkan:
                *outVertex = Vulkan::wireframe_vert();
                *outFragment = Vulkan::wireframe_frag();
                return true;
            case GraphicsBackend::DirectX11:
                *outVertex = DirectX11::wireframe_vert();
                *outFragment = DirectX11::wireframe_frag();
                return true;
            case GraphicsBackend::DirectX12:
                *outVertex = DirectX12::wireframe_vert();
                *outFragment = DirectX12::wireframe_frag();
                return true;
            case GraphicsBackend::Metal:
                *outVertex = Metal::wireframe_metal();
                *outFragment = Metal::wireframe_metal();
                return true;
            case GraphicsBackend::WebGL:
                *outVertex = WebGL::wireframe_vert();
                *outFragment = WebGL::wireframe_frag();
                return true;
            default:
                return false;
        }
    }
    else if (strcmp(shaderName, "Line") == 0) {
        switch (backend) {
            case GraphicsBackend::OpenGL:
                *outVertex = OpenGL::line_vert();
                *outFragment = OpenGL::line_frag();
                return true;
            case GraphicsBackend::Vulkan:
                *outVertex = Vulkan::line_vert();
                *outFragment = Vulkan::line_frag();
                return true;
            case GraphicsBackend::DirectX11:
                *outVertex = DirectX11::line_vert();
                *outFragment = DirectX11::line_frag();
                return true;
            case GraphicsBackend::DirectX12:
                *outVertex = DirectX12::line_vert();
                *outFragment = DirectX12::line_frag();
                return true;
            case GraphicsBackend::Metal:
                *outVertex = Metal::line_metal();
                *outFragment = Metal::line_metal();
                return true;
            case GraphicsBackend::WebGL:
                *outVertex = WebGL::line_vert();
                *outFragment = WebGL::line_frag();
                return true;
            default:
                return false;
        }
    }
    else if (strcmp(shaderName, "ShadowMap") == 0) {
        switch (backend) {
            case GraphicsBackend::OpenGL:
                *outVertex = OpenGL::shadow_map_vert();
                *outFragment = OpenGL::shadow_map_frag();
                return true;
            case GraphicsBackend::Vulkan:
                *outVertex = Vulkan::shadow_map_vert();
                *outFragment = Vulkan::shadow_map_frag();
                return true;
            case GraphicsBackend::DirectX11:
                *outVertex = DirectX11::shadow_map_vert();
                *outFragment = DirectX11::shadow_map_frag();
                return true;
            case GraphicsBackend::DirectX12:
                *outVertex = DirectX12::shadow_map_vert();
                *outFragment = DirectX12::shadow_map_frag();
                return true;
            case GraphicsBackend::Metal:
                *outVertex = Metal::shadow_map_metal();
                *outFragment = Metal::shadow_map_metal();
                return true;
            case GraphicsBackend::WebGL:
                *outVertex = WebGL::shadow_map_vert();
                *outFragment = WebGL::shadow_map_frag();
                return true;
            default:
                return false;
        }
    }
    else if (strcmp(shaderName, "RoundedRect") == 0) {
        switch (backend) {
            case GraphicsBackend::OpenGL:
                *outVertex = OpenGL::rounded_rect_vert();
                *outFragment = OpenGL::rounded_rect_frag();
                return true;
            case GraphicsBackend::Vulkan:
                *outVertex = Vulkan::rounded_rect_vert();
                *outFragment = Vulkan::rounded_rect_frag();
                return true;
            case GraphicsBackend::DirectX11:
                *outVertex = DirectX11::rounded_rect_vert();
                *outFragment = DirectX11::rounded_rect_frag();
                return true;
            case GraphicsBackend::DirectX12:
                *outVertex = DirectX12::rounded_rect_vert();
                *outFragment = DirectX12::rounded_rect_frag();
                return true;
            case GraphicsBackend::Metal:
                *outVertex = Metal::rounded_rect_metal();
                *outFragment = Metal::rounded_rect_metal();
                return true;
            case GraphicsBackend::WebGL:
                *outVertex = WebGL::rounded_rect_vert();
                *outFragment = WebGL::rounded_rect_frag();
                return true;
            default:
                return false;
        }
    }
    else if (strcmp(shaderName, "RoundedRectBorder") == 0) {
        switch (backend) {
            case GraphicsBackend::OpenGL:
                *outVertex = OpenGL::rounded_rect_border_vert();
                *outFragment = OpenGL::rounded_rect_border_frag();
                return true;
            case GraphicsBackend::Vulkan:
                *outVertex = Vulkan::rounded_rect_border_vert();
                *outFragment = Vulkan::rounded_rect_border_frag();
                return true;
            case GraphicsBackend::DirectX11:
                *outVertex = DirectX11::rounded_rect_border_vert();
                *outFragment = DirectX11::rounded_rect_border_frag();
                return true;
            case GraphicsBackend::DirectX12:
                *outVertex = DirectX12::rounded_rect_border_vert();
                *outFragment = DirectX12::rounded_rect_border_frag();
                return true;
            case GraphicsBackend::Metal:
                *outVertex = Metal::rounded_rect_border_metal();
                *outFragment = Metal::rounded_rect_border_metal();
                return true;
            case GraphicsBackend::WebGL:
                *outVertex = WebGL::rounded_rect_border_vert();
                *outFragment = WebGL::rounded_rect_border_frag();
                return true;
            default:
                return false;
        }
    }
    else if (strcmp(shaderName, "RadialGradient") == 0) {
        switch (backend) {
            case GraphicsBackend::OpenGL:
                *outVertex = OpenGL::radial_gradient_vert();
                *outFragment = OpenGL::radial_gradient_frag();
                return true;
            case GraphicsBackend::Vulkan:
                *outVertex = Vulkan::radial_gradient_vert();
                *outFragment = Vulkan::radial_gradient_frag();
                return true;
            case GraphicsBackend::DirectX11:
                *outVertex = DirectX11::radial_gradient_vert();
                *outFragment = DirectX11::radial_gradient_frag();
                return true;
            case GraphicsBackend::DirectX12:
                *outVertex = DirectX12::radial_gradient_vert();
                *outFragment = DirectX12::radial_gradient_frag();
                return true;
            case GraphicsBackend::Metal:
                *outVertex = Metal::radial_gradient_metal();
                *outFragment = Metal::radial_gradient_metal();
                return true;
            case GraphicsBackend::WebGL:
                *outVertex = WebGL::radial_gradient_vert();
                *outFragment = WebGL::radial_gradient_frag();
                return true;
            default:
                return false;
        }
    }
    else if (strcmp(shaderName, "Polygon") == 0) {
        switch (backend) {
            case GraphicsBackend::OpenGL:
                *outVertex = OpenGL::polygon_vert();
                *outFragment = OpenGL::polygon_frag();
                return true;
            case GraphicsBackend::Vulkan:
                *outVertex = Vulkan::polygon_vert();
                *outFragment = Vulkan::polygon_frag();
                return true;
            case GraphicsBackend::DirectX11:
                *outVertex = DirectX11::polygon_vert();
                *outFragment = DirectX11::polygon_frag();
                return true;
            case GraphicsBackend::DirectX12:
                *outVertex = DirectX12::polygon_vert();
                *outFragment = DirectX12::polygon_frag();
                return true;
            case GraphicsBackend::Metal:
                *outVertex = Metal::polygon_metal();
                *outFragment = Metal::polygon_metal();
                return true;
            case GraphicsBackend::WebGL:
                *outVertex = WebGL::polygon_vert();
                *outFragment = WebGL::polygon_frag();
                return true;
            default:
                return false;
        }
    }
    else if (strcmp(shaderName, "Skybox") == 0) {
        switch (backend) {
            case GraphicsBackend::OpenGL:
                *outVertex = OpenGL::skybox_vert();
                *outFragment = OpenGL::skybox_frag();
                return true;
            case GraphicsBackend::Vulkan:
                *outVertex = Vulkan::skybox_vert();
                *outFragment = Vulkan::skybox_frag();
                return true;
            case GraphicsBackend::DirectX11:
                *outVertex = DirectX11::skybox_vert();
                *outFragment = DirectX11::skybox_frag();
                return true;
            case GraphicsBackend::DirectX12:
                *outVertex = DirectX12::skybox_vert();
                *outFragment = DirectX12::skybox_frag();
                return true;
            case GraphicsBackend::Metal:
                *outVertex = Metal::skybox_metal();
                *outFragment = Metal::skybox_metal();
                return true;
            case GraphicsBackend::WebGL:
                *outVertex = WebGL::skybox_vert();
                *outFragment = WebGL::skybox_frag();
                return true;
            default:
                return false;
        }
    }
    else if (strcmp(shaderName, "Toon") == 0) {
        switch (backend) {
            case GraphicsBackend::OpenGL:
                *outVertex = OpenGL::toon_vert();
                *outFragment = OpenGL::toon_frag();
                return true;
            case GraphicsBackend::Vulkan:
                *outVertex = Vulkan::toon_vert();
                *outFragment = Vulkan::toon_frag();
                return true;
            case GraphicsBackend::DirectX11:
                *outVertex = DirectX11::toon_vert();
                *outFragment = DirectX11::toon_frag();
                return true;
            case GraphicsBackend::DirectX12:
                *outVertex = DirectX12::toon_vert();
                *outFragment = DirectX12::toon_frag();
                return true;
            case GraphicsBackend::Metal:
                *outVertex = Metal::toon_metal();
                *outFragment = Metal::toon_metal();
                return true;
            case GraphicsBackend::WebGL:
                *outVertex = WebGL::toon_vert();
                *outFragment = WebGL::toon_frag();
                return true;
            default:
                return false;
        }
    }
    else if (strcmp(shaderName, "SkeletalToon") == 0) {
        switch (backend) {
            case GraphicsBackend::OpenGL:
                *outVertex = OpenGL::skeletal_toon_vert();
                *outFragment = OpenGL::toon_frag();  // Shares fragment shader
                return true;
            case GraphicsBackend::Vulkan:
                *outVertex = Vulkan::skeletal_toon_vert();
                *outFragment = Vulkan::toon_frag();  // Shares fragment shader
                return true;
            case GraphicsBackend::DirectX11:
                *outVertex = DirectX11::skeletal_toon_vert();
                *outFragment = DirectX11::toon_frag();  // Shares fragment shader
                return true;
            case GraphicsBackend::DirectX12:
                *outVertex = DirectX12::skeletal_toon_vert();
                *outFragment = DirectX12::toon_frag();  // Shares fragment shader
                return true;
            case GraphicsBackend::Metal:
                *outVertex = Metal::skeletal_toon_metal();
                *outFragment = Metal::skeletal_toon_metal();
                return true;
            case GraphicsBackend::WebGL:
                *outVertex = WebGL::skeletal_toon_vert();
                *outFragment = WebGL::toon_frag();  // Shares fragment shader
                return true;
            default:
                return false;
        }
    }
    else if (strcmp(shaderName, "Stylized") == 0) {
        switch (backend) {
            case GraphicsBackend::OpenGL:
                *outVertex = OpenGL::stylized_vert();
                *outFragment = OpenGL::stylized_frag();
                return true;
            case GraphicsBackend::Vulkan:
                *outVertex = Vulkan::stylized_vert();
                *outFragment = Vulkan::stylized_frag();
                return true;
            case GraphicsBackend::DirectX11:
                *outVertex = DirectX11::stylized_vert();
                *outFragment = DirectX11::stylized_frag();
                return true;
            case GraphicsBackend::DirectX12:
                *outVertex = DirectX12::stylized_vert();
                *outFragment = DirectX12::stylized_frag();
                return true;
            case GraphicsBackend::Metal:
                *outVertex = Metal::stylized_metal();
                *outFragment = Metal::stylized_metal();
                return true;
            case GraphicsBackend::WebGL:
                *outVertex = WebGL::stylized_vert();
                *outFragment = WebGL::stylized_frag();
                return true;
            default:
                return false;
        }
    }
    else if (strcmp(shaderName, "SkeletalStylized") == 0) {
        switch (backend) {
            case GraphicsBackend::OpenGL:
                *outVertex = OpenGL::skeletal_stylized_vert();
                *outFragment = OpenGL::stylized_frag();  // Shares fragment shader
                return true;
            case GraphicsBackend::Vulkan:
                *outVertex = Vulkan::skeletal_stylized_vert();
                *outFragment = Vulkan::stylized_frag();  // Shares fragment shader
                return true;
            case GraphicsBackend::DirectX11:
                *outVertex = DirectX11::skeletal_stylized_vert();
                *outFragment = DirectX11::stylized_frag();  // Shares fragment shader
                return true;
            case GraphicsBackend::DirectX12:
                *outVertex = DirectX12::skeletal_stylized_vert();
                *outFragment = DirectX12::stylized_frag();  // Shares fragment shader
                return true;
            case GraphicsBackend::Metal:
                *outVertex = Metal::skeletal_stylized_metal();
                *outFragment = Metal::skeletal_stylized_metal();
                return true;
            case GraphicsBackend::WebGL:
                *outVertex = WebGL::skeletal_stylized_vert();
                *outFragment = WebGL::stylized_frag();  // Shares fragment shader
                return true;
            default:
                return false;
        }
    }
    else if (strcmp(shaderName, "Transparent") == 0) {
        switch (backend) {
            case GraphicsBackend::OpenGL:
                *outVertex = OpenGL::transparent_vert();
                *outFragment = OpenGL::transparent_frag();
                return true;
            case GraphicsBackend::Vulkan:
                *outVertex = Vulkan::transparent_vert();
                *outFragment = Vulkan::transparent_frag();
                return true;
            case GraphicsBackend::DirectX11:
                *outVertex = DirectX11::transparent_vert();
                *outFragment = DirectX11::transparent_frag();
                return true;
            case GraphicsBackend::DirectX12:
                *outVertex = DirectX12::transparent_vert();
                *outFragment = DirectX12::transparent_frag();
                return true;
            case GraphicsBackend::Metal:
                *outVertex = Metal::transparent_metal();
                *outFragment = Metal::transparent_metal();
                return true;
            case GraphicsBackend::WebGL:
                *outVertex = WebGL::transparent_vert();
                *outFragment = WebGL::transparent_frag();
                return true;
            default:
                return false;
        }
    }
    else if (strcmp(shaderName, "Glow") == 0) {
        switch (backend) {
            case GraphicsBackend::OpenGL:
                *outVertex = OpenGL::glow_vert();
                *outFragment = OpenGL::glow_frag();
                return true;
            case GraphicsBackend::Vulkan:
                *outVertex = Vulkan::glow_vert();
                *outFragment = Vulkan::glow_frag();
                return true;
            case GraphicsBackend::DirectX11:
                *outVertex = DirectX11::glow_vert();
                *outFragment = DirectX11::glow_frag();
                return true;
            case GraphicsBackend::DirectX12:
                *outVertex = DirectX12::glow_vert();
                *outFragment = DirectX12::glow_frag();
                return true;
            case GraphicsBackend::Metal:
                *outVertex = Metal::glow_metal();
                *outFragment = Metal::glow_metal();
                return true;
            case GraphicsBackend::WebGL:
                *outVertex = WebGL::glow_vert();
                *outFragment = WebGL::glow_frag();
                return true;
            default:
                return false;
        }
    }
    else if (strcmp(shaderName, "ShadowCube") == 0) {
        switch (backend) {
            case GraphicsBackend::OpenGL:
                *outVertex = OpenGL::shadow_cube_vert();
                *outFragment = OpenGL::shadow_cube_frag();
                return true;
            case GraphicsBackend::Vulkan:
                *outVertex = Vulkan::shadow_cube_vert();
                *outFragment = Vulkan::shadow_cube_frag();
                return true;
            case GraphicsBackend::DirectX11:
                *outVertex = DirectX11::shadow_cube_vert();
                *outFragment = DirectX11::shadow_cube_frag();
                return true;
            case GraphicsBackend::DirectX12:
                *outVertex = DirectX12::shadow_cube_vert();
                *outFragment = DirectX12::shadow_cube_frag();
                return true;
            case GraphicsBackend::Metal:
                *outVertex = Metal::shadow_cube_metal();
                *outFragment = Metal::shadow_cube_metal();
                return true;
            case GraphicsBackend::WebGL:
                *outVertex = WebGL::shadow_cube_vert();
                *outFragment = WebGL::shadow_cube_frag();
                return true;
            default:
                return false;
        }
    }
    else if (strcmp(shaderName, "ShadowMapSkeletal") == 0) {
        switch (backend) {
            case GraphicsBackend::OpenGL:
                *outVertex = OpenGL::shadow_map_skeletal_vert();
                *outFragment = OpenGL::shadow_map_frag();  // Shares fragment shader with ShadowMap
                return true;
            case GraphicsBackend::Vulkan:
                *outVertex = Vulkan::shadow_map_skeletal_vert();
                *outFragment = Vulkan::shadow_map_frag();  // Shares fragment shader with ShadowMap
                return true;
            case GraphicsBackend::DirectX11:
                *outVertex = DirectX11::shadow_map_skeletal_vert();
                *outFragment = DirectX11::shadow_map_frag();  // Shares fragment shader with ShadowMap
                return true;
            case GraphicsBackend::DirectX12:
                *outVertex = DirectX12::shadow_map_skeletal_vert();
                *outFragment = DirectX12::shadow_map_frag();  // Shares fragment shader with ShadowMap
                return true;
            case GraphicsBackend::Metal:
                *outVertex = Metal::shadow_map_skeletal_metal();
                *outFragment = Metal::shadow_map_skeletal_metal();
                return true;
            case GraphicsBackend::WebGL:
                *outVertex = WebGL::shadow_map_skeletal_vert();
                *outFragment = WebGL::shadow_map_frag();  // Shares fragment shader with ShadowMap
                return true;
            default:
                return false;
        }
    }

    // --- DX-only geometry-stage debug shaders ---
    // These shaders use geometry stages for line thickening and are only
    // available on DirectX11 and DirectX12 backends.  Other backends
    // (OpenGL, Vulkan, WebGL, Metal) return false.
    else if (strcmp(shaderName, "BoundingBox") == 0) {
        switch (backend) {
            case GraphicsBackend::DirectX11:
                *outVertex = DirectX11::bounding_box_vert();
                *outFragment = DirectX11::bounding_box_frag();
                return true;
            case GraphicsBackend::DirectX12:
                *outVertex = DirectX12::bounding_box_vert();
                *outFragment = DirectX12::bounding_box_frag();
                return true;
            default:
                return false;
        }
    }
    else if (strcmp(shaderName, "Gizmo") == 0) {
        switch (backend) {
            case GraphicsBackend::DirectX11:
                *outVertex = DirectX11::gizmo_vert();
                *outFragment = DirectX11::gizmo_frag();
                return true;
            case GraphicsBackend::DirectX12:
                *outVertex = DirectX12::gizmo_vert();
                *outFragment = DirectX12::gizmo_frag();
                return true;
            default:
                return false;
        }
    }
    else if (strcmp(shaderName, "ThickLine") == 0) {
        switch (backend) {
            case GraphicsBackend::DirectX11:
                *outVertex = DirectX11::thick_line_vert();
                *outFragment = DirectX11::thick_line_frag();
                return true;
            case GraphicsBackend::DirectX12:
                *outVertex = DirectX12::thick_line_vert();
                *outFragment = DirectX12::thick_line_frag();
                return true;
            default:
                return false;
        }
    }

    return false;
}

/**
 * Get geometry shader source for shaders that have a geometry stage.
 * Only DX11/DX12 backends support these debug shaders.
 */
inline bool getGeometryShader(const char* shaderName, GraphicsBackend backend,
                               const char** outGeometry) {
    using namespace GeneratedShaders;

    if (strcmp(shaderName, "BoundingBox") == 0) {
        switch (backend) {
            case GraphicsBackend::DirectX11:
                *outGeometry = DirectX11::bounding_box_geom();
                return true;
            case GraphicsBackend::DirectX12:
                *outGeometry = DirectX12::bounding_box_geom();
                return true;
            default: return false;
        }
    }
    else if (strcmp(shaderName, "Gizmo") == 0) {
        switch (backend) {
            case GraphicsBackend::DirectX11:
                *outGeometry = DirectX11::gizmo_geom();
                return true;
            case GraphicsBackend::DirectX12:
                *outGeometry = DirectX12::gizmo_geom();
                return true;
            default: return false;
        }
    }
    else if (strcmp(shaderName, "ThickLine") == 0) {
        switch (backend) {
            case GraphicsBackend::DirectX11:
                *outGeometry = DirectX11::thick_line_geom();
                return true;
            case GraphicsBackend::DirectX12:
                *outGeometry = DirectX12::thick_line_geom();
                return true;
            default: return false;
        }
    }
    return false;
}

/**
 * Get precompiled shader bytecode for DirectX12 (DXIL SM 6.0)
 * This eliminates the need for runtime DXC dependency.
 *
 * @param shaderName Name of the shader (e.g., "Standard3D", "PBR", etc.)
 * @param outBytecode Pointer to receive shader bytecode pair
 * @return true if precompiled bytecode is available, false otherwise
 *
 * Note: Only DirectX12 supports precompiled bytecode. Other backends
 * use runtime compilation from source.
 */
inline bool getShaderBytecode(const char* shaderName, ShaderBytecodePair* outBytecode) {
#ifdef LUPINE_HAS_DIRECTX12
    using namespace GeneratedShaders::DirectX12;

    // Helper macro to reduce repetition for standard shaders
    #define DX12_BYTECODE_SHADER(name, prefix) \
        if (strcmp(shaderName, name) == 0) { \
            outBytecode->vertex = { prefix##_vert_bytecode(), prefix##_vert_bytecode_size() }; \
            outBytecode->fragment = { prefix##_frag_bytecode(), prefix##_frag_bytecode_size() }; \
            return true; \
        }

    #define DX12_BYTECODE_SHADER_GEOM(name, prefix) \
        if (strcmp(shaderName, name) == 0) { \
            outBytecode->vertex = { prefix##_vert_bytecode(), prefix##_vert_bytecode_size() }; \
            outBytecode->fragment = { prefix##_frag_bytecode(), prefix##_frag_bytecode_size() }; \
            outBytecode->geometry = { prefix##_geom_bytecode(), prefix##_geom_bytecode_size() }; \
            return true; \
        }

    DX12_BYTECODE_SHADER("Standard3D", standard3_d)
    DX12_BYTECODE_SHADER("PBR", pbr)
    DX12_BYTECODE_SHADER("PBRInstanced", pbr_instanced)
    DX12_BYTECODE_SHADER("ShadowMapInstanced", shadow_map_instanced)
    DX12_BYTECODE_SHADER("ShadowCubeInstanced", shadow_cube_instanced)
    DX12_BYTECODE_SHADER("Skeletal", skeletal)
    DX12_BYTECODE_SHADER("Sprite2D", sprite2_d)
    DX12_BYTECODE_SHADER("Sprite2D_AlphaCutoff", sprite2_d_alpha_cutoff)
    DX12_BYTECODE_SHADER("Sprite3D", sprite3_d)
    DX12_BYTECODE_SHADER("Text", text)
    DX12_BYTECODE_SHADER("Text3D", text3_d)
    DX12_BYTECODE_SHADER("Unlit", unlit)
    DX12_BYTECODE_SHADER("Wireframe", wireframe)
    DX12_BYTECODE_SHADER("Line", line)
    DX12_BYTECODE_SHADER("ShadowMap", shadow_map)
    DX12_BYTECODE_SHADER("ShadowCube", shadow_cube)
    DX12_BYTECODE_SHADER("RoundedRect", rounded_rect)
    DX12_BYTECODE_SHADER("RoundedRectBorder", rounded_rect_border)
    DX12_BYTECODE_SHADER("RadialGradient", radial_gradient)
    DX12_BYTECODE_SHADER("Polygon", polygon)
    DX12_BYTECODE_SHADER("Skybox", skybox)
    DX12_BYTECODE_SHADER("Toon", toon)
    DX12_BYTECODE_SHADER("Stylized", stylized)
    DX12_BYTECODE_SHADER("Transparent", transparent)
    DX12_BYTECODE_SHADER("Glow", glow)

    // BoundingBox, Gizmo, ThickLine - DX-specific debug shaders with geometry stages
    DX12_BYTECODE_SHADER_GEOM("BoundingBox", bounding_box)
    DX12_BYTECODE_SHADER_GEOM("Gizmo", gizmo)
    DX12_BYTECODE_SHADER_GEOM("ThickLine", thick_line)

    // Skeletal variants (share fragment shader)
    if (strcmp(shaderName, "SkeletalToon") == 0) {
        outBytecode->vertex = { skeletal_toon_vert_bytecode(), skeletal_toon_vert_bytecode_size() };
        outBytecode->fragment = { toon_frag_bytecode(), toon_frag_bytecode_size() };
        return true;
    }
    if (strcmp(shaderName, "SkeletalStylized") == 0) {
        outBytecode->vertex = { skeletal_stylized_vert_bytecode(), skeletal_stylized_vert_bytecode_size() };
        outBytecode->fragment = { stylized_frag_bytecode(), stylized_frag_bytecode_size() };
        return true;
    }
    if (strcmp(shaderName, "ShadowMapSkeletal") == 0) {
        outBytecode->vertex = { shadow_map_skeletal_vert_bytecode(), shadow_map_skeletal_vert_bytecode_size() };
        outBytecode->fragment = { shadow_map_frag_bytecode(), shadow_map_frag_bytecode_size() };
        return true;
    }

    #undef DX12_BYTECODE_SHADER
    #undef DX12_BYTECODE_SHADER_GEOM
#else
    (void)shaderName;
    (void)outBytecode;
#endif

    return false;
}

/**
 * Check if a backend supports precompiled shader bytecode
 * @param backend Graphics backend to check
 * @return true if the backend supports precompiled bytecode
 */
inline bool supportsPrecompiledBytecode(GraphicsBackend backend) {
    return backend == GraphicsBackend::DirectX12;
}

/**
 * Shader data container that can hold either source code or bytecode
 * This allows unified handling of shaders across backends
 */
struct ShaderData {
    const void* data = nullptr;
    size_t size = 0;
    bool isPrecompiled = false;

    bool isValid() const { return data != nullptr && size > 0; }
};

/**
 * Container for vertex and fragment shader data
 */
struct ShaderDataPair {
    ShaderData vertex;
    ShaderData fragment;
    ShaderData geometry;  // Optional

    bool hasVertex() const { return vertex.isValid(); }
    bool hasFragment() const { return fragment.isValid(); }
    bool hasGeometry() const { return geometry.isValid(); }
};

/**
 * Get shader data (precompiled bytecode for DX12, source for others)
 * This is the preferred API for creating shaders as it automatically
 * uses precompiled bytecode when available.
 *
 * @param shaderName Name of the shader (e.g., "Standard3D", "PBR", etc.)
 * @param backend Graphics backend to get shaders for
 * @param outData Pointer to receive shader data
 * @return true if shader was found, false otherwise
 */
inline bool getShaderData(const char* shaderName, GraphicsBackend backend, ShaderDataPair* outData) {
    if (!outData) return false;

    *outData = ShaderDataPair{};  // Clear output

#ifdef LUPINE_HAS_DIRECTX12
    // For DirectX12, try to get precompiled bytecode first
    if (backend == GraphicsBackend::DirectX12) {
        ShaderBytecodePair bytecode;
        if (getShaderBytecode(shaderName, &bytecode)) {
            if (bytecode.hasPrecompiledVertex()) {
                outData->vertex.data = bytecode.vertex.data;
                outData->vertex.size = bytecode.vertex.size;
                outData->vertex.isPrecompiled = true;
            }
            if (bytecode.hasPrecompiledFragment()) {
                outData->fragment.data = bytecode.fragment.data;
                outData->fragment.size = bytecode.fragment.size;
                outData->fragment.isPrecompiled = true;
            }
            if (bytecode.hasPrecompiledGeometry()) {
                outData->geometry.data = bytecode.geometry.data;
                outData->geometry.size = bytecode.geometry.size;
                outData->geometry.isPrecompiled = true;
            }

            // If we have both vertex and fragment, we're done
            if (outData->hasVertex() && outData->hasFragment()) {
                return true;
            }
        }
    }
#endif

    // Fall back to source code
    const char* vertSource = nullptr;
    const char* fragSource = nullptr;
    if (getShader(shaderName, backend, &vertSource, &fragSource)) {
        outData->vertex.data = vertSource;
        outData->vertex.size = vertSource ? std::strlen(vertSource) : 0;
        outData->vertex.isPrecompiled = false;

        outData->fragment.data = fragSource;
        outData->fragment.size = fragSource ? std::strlen(fragSource) : 0;
        outData->fragment.isPrecompiled = false;

        // Also try to get geometry shader source if one exists
        const char* geomSource = nullptr;
        if (getGeometryShader(shaderName, backend, &geomSource)) {
            outData->geometry.data = geomSource;
            outData->geometry.size = geomSource ? std::strlen(geomSource) : 0;
            outData->geometry.isPrecompiled = false;
        }

        return outData->hasVertex() && outData->hasFragment();
    }

    return false;
}

/**
 * Get the appropriate entry point name for vertex shaders based on backend
 * Metal uses vertex_main, others use main
 */
inline const char* getVertexEntryPoint(GraphicsBackend backend) {
    return (backend == GraphicsBackend::Metal) ? "vertex_main" : "main";
}

/**
 * Get the appropriate entry point name for fragment shaders based on backend
 * Metal uses fragment_main, others use main
 */
inline const char* getFragmentEntryPoint(GraphicsBackend backend) {
    return (backend == GraphicsBackend::Metal) ? "fragment_main" : "main";
}

/**
 * Translate a custom .lsh shader source to the current backend.
 * Returns true if translation succeeded.
 */
inline bool translateCustomShader(const std::string& lshSource, GraphicsBackend backend,
                                   ShaderDataPair* outData,
                                   std::string& outVertexSource, std::string& outFragmentSource,
                                   std::string* outGeometrySource = nullptr) {
    if (!outData) return false;
    *outData = ShaderDataPair{};

    ShaderTranslatorResult result = ShaderTranslator::translate(lshSource, backend);
    if (!result.success) return false;

    outVertexSource = std::move(result.vertexSource);
    outFragmentSource = std::move(result.fragmentSource);

    // For Metal, both vertex and fragment come from combined source
    if (backend == GraphicsBackend::Metal) {
        outVertexSource = result.combinedSource;
        outFragmentSource = result.combinedSource;
    }

    outData->vertex.data = outVertexSource.c_str();
    outData->vertex.size = outVertexSource.size();
    outData->vertex.isPrecompiled = false;
    outData->fragment.data = outFragmentSource.c_str();
    outData->fragment.size = outFragmentSource.size();
    outData->fragment.isPrecompiled = false;

    // Geometry stage (DirectX11/DirectX12 only)
    if (outGeometrySource && !result.geometrySource.empty()) {
        *outGeometrySource = std::move(result.geometrySource);
        outData->geometry.data = outGeometrySource->c_str();
        outData->geometry.size = outGeometrySource->size();
        outData->geometry.isPrecompiled = false;
    }

    return true;
}

} // namespace DefaultShaders
} // namespace lupine
