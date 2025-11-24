#pragma once

#include "GeneratedShaders.hpp"
#include "gfx/GfxTypes.hpp"
#include <cstring>

namespace lupine {
namespace DefaultShaders {

/**
 * Backend-agnostic shader access
 * Provides shader sources that automatically select the appropriate backend
 * based on the current GraphicsBackend in use.
 */

// Shader type helper
struct ShaderPair {
    const char* vertex;
    const char* fragment;
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
            case GraphicsBackend::WebGL:
                *outVertex = OpenGL::standard3_d_vert;
                *outFragment = OpenGL::standard3_d_frag;
                return true;
            // Add other backends when implemented
            default:
                return false;
        }
    }
    else if (strcmp(shaderName, "PBR") == 0) {
        switch (backend) {
            case GraphicsBackend::OpenGL:
            case GraphicsBackend::WebGL:
                *outVertex = OpenGL::p_b_r_vert;
                *outFragment = OpenGL::p_b_r_frag();
                return true;
            default:
                return false;
        }
    }
    else if (strcmp(shaderName, "Skeletal") == 0) {
        switch (backend) {
            case GraphicsBackend::OpenGL:
            case GraphicsBackend::WebGL:
                *outVertex = OpenGL::skeletal_vert;
                *outFragment = OpenGL::skeletal_frag();
                return true;
            default:
                return false;
        }
    }
    else if (strcmp(shaderName, "Sprite2D") == 0) {
        switch (backend) {
            case GraphicsBackend::OpenGL:
            case GraphicsBackend::WebGL:
                *outVertex = OpenGL::sprite2_d_vert;
                *outFragment = OpenGL::sprite2_d_frag;
                return true;
            default:
                return false;
        }
    }
    else if (strcmp(shaderName, "Sprite2D_AlphaCutoff") == 0) {
        switch (backend) {
            case GraphicsBackend::OpenGL:
            case GraphicsBackend::WebGL:
                *outVertex = OpenGL::sprite2_d__alpha_cutoff_vert;
                *outFragment = OpenGL::sprite2_d__alpha_cutoff_frag;
                return true;
            default:
                return false;
        }
    }
    else if (strcmp(shaderName, "Sprite3D") == 0) {
        switch (backend) {
            case GraphicsBackend::OpenGL:
            case GraphicsBackend::WebGL:
                *outVertex = OpenGL::sprite3_d_vert;
                *outFragment = OpenGL::sprite3_d_frag;
                return true;
            default:
                return false;
        }
    }
    else if (strcmp(shaderName, "Text") == 0) {
        switch (backend) {
            case GraphicsBackend::OpenGL:
            case GraphicsBackend::WebGL:
                *outVertex = OpenGL::text_vert;
                *outFragment = OpenGL::text_frag;
                return true;
            default:
                return false;
        }
    }
    else if (strcmp(shaderName, "Text3D") == 0) {
        switch (backend) {
            case GraphicsBackend::OpenGL:
            case GraphicsBackend::WebGL:
                *outVertex = OpenGL::text3d_vert;
                *outFragment = OpenGL::text3d_frag;
                return true;
            default:
                return false;
        }
    }
    else if (strcmp(shaderName, "Unlit") == 0) {
        switch (backend) {
            case GraphicsBackend::OpenGL:
            case GraphicsBackend::WebGL:
                *outVertex = OpenGL::unlit_vert;
                *outFragment = OpenGL::unlit_frag;
                return true;
            default:
                return false;
        }
    }
    else if (strcmp(shaderName, "Wireframe") == 0) {
        switch (backend) {
            case GraphicsBackend::OpenGL:
            case GraphicsBackend::WebGL:
                *outVertex = OpenGL::wireframe_vert;
                *outFragment = OpenGL::wireframe_frag;
                return true;
            default:
                return false;
        }
    }
    else if (strcmp(shaderName, "Line") == 0) {
        switch (backend) {
            case GraphicsBackend::OpenGL:
            case GraphicsBackend::WebGL:
                *outVertex = OpenGL::line_vert;
                *outFragment = OpenGL::line_frag;
                return true;
            default:
                return false;
        }
    }
    else if (strcmp(shaderName, "ShadowMap") == 0) {
        switch (backend) {
            case GraphicsBackend::OpenGL:
            case GraphicsBackend::WebGL:
                *outVertex = OpenGL::shadow_map_vert;
                *outFragment = OpenGL::shadow_map_frag;
                return true;
            default:
                return false;
        }
    }
    else if (strcmp(shaderName, "RoundedRect") == 0) {
        switch (backend) {
            case GraphicsBackend::OpenGL:
            case GraphicsBackend::WebGL:
                *outVertex = OpenGL::rounded_rect_vert;
                *outFragment = OpenGL::rounded_rect_frag;
                return true;
            default:
                return false;
        }
    }
    else if (strcmp(shaderName, "RoundedRectBorder") == 0) {
        switch (backend) {
            case GraphicsBackend::OpenGL:
            case GraphicsBackend::WebGL:
                *outVertex = OpenGL::rounded_rect_border_vert;
                *outFragment = OpenGL::rounded_rect_border_frag;
                return true;
            default:
                return false;
        }
    }
    else if (strcmp(shaderName, "Skybox") == 0) {
        switch (backend) {
            case GraphicsBackend::OpenGL:
            case GraphicsBackend::WebGL:
                *outVertex = OpenGL::skybox_vert;
                *outFragment = OpenGL::skybox_frag;
                return true;
            default:
                return false;
        }
    }

    return false;
}

// ===== Uniform Shader Accessor Functions =====
// All shaders are accessed through inline functions for consistency
// This ensures the API is the same regardless of shader size or backend

namespace OpenGL = GeneratedShaders::OpenGL;

// Standard 3D shaders
inline const char* Standard3D_Vertex() { return GeneratedShaders::OpenGL::standard3_d_vert; }
inline const char* Standard3D_Fragment() { return GeneratedShaders::OpenGL::standard3_d_frag; }

// PBR shaders
inline const char* PBR_Vertex() { return GeneratedShaders::OpenGL::p_b_r_vert; }
inline const char* PBR_Fragment() { return GeneratedShaders::OpenGL::p_b_r_frag(); }

// Skeletal animation shaders
inline const char* Skeletal_Vertex() { return GeneratedShaders::OpenGL::skeletal_vert; }
inline const char* Skeletal_Fragment() { return GeneratedShaders::OpenGL::skeletal_frag(); }

// 2D Sprite shaders
inline const char* Sprite2D_Vertex() { return GeneratedShaders::OpenGL::sprite2_d_vert; }
inline const char* Sprite2D_Fragment() { return GeneratedShaders::OpenGL::sprite2_d_frag; }

inline const char* Sprite2D_AlphaCutoff_Vertex() { return GeneratedShaders::OpenGL::sprite2_d__alpha_cutoff_vert; }
inline const char* Sprite2D_AlphaCutoff_Fragment() { return GeneratedShaders::OpenGL::sprite2_d__alpha_cutoff_frag; }

// 3D Sprite shaders
inline const char* Sprite3D_Vertex() { return GeneratedShaders::OpenGL::sprite3_d_vert; }
inline const char* Sprite3D_Fragment() { return GeneratedShaders::OpenGL::sprite3_d_frag; }

// Text rendering shaders
inline const char* Text_Vertex() { return GeneratedShaders::OpenGL::text_vert; }
inline const char* Text_Fragment() { return GeneratedShaders::OpenGL::text_frag; }

// 3D Text rendering shaders
inline const char* Text3D_Vertex() { return GeneratedShaders::OpenGL::text3d_vert; }
inline const char* Text3D_Fragment() { return GeneratedShaders::OpenGL::text3d_frag; }

// Unlit shaders
inline const char* Unlit_Vertex() { return GeneratedShaders::OpenGL::unlit_vert; }
inline const char* Unlit_Fragment() { return GeneratedShaders::OpenGL::unlit_frag; }

// Wireframe shaders
inline const char* Wireframe_Vertex() { return GeneratedShaders::OpenGL::wireframe_vert; }
inline const char* Wireframe_Fragment() { return GeneratedShaders::OpenGL::wireframe_frag; }

// Line rendering shaders
inline const char* Line_Vertex() { return GeneratedShaders::OpenGL::line_vert; }
inline const char* Line_Fragment() { return GeneratedShaders::OpenGL::line_frag; }

// Shadow map shaders
inline const char* ShadowMap_Vertex() { return GeneratedShaders::OpenGL::shadow_map_vert; }
inline const char* ShadowMap_Fragment() { return GeneratedShaders::OpenGL::shadow_map_frag; }

// Skeletal shadow map shader
inline const char* ShadowMapSkeletal_Vertex() { return GeneratedShaders::OpenGL::shadow_map_skeletal_vert; }

// Shadow cube map shaders (for point lights)
inline const char* ShadowCube_Vertex() { return GeneratedShaders::OpenGL::shadow_cube_vert; }
inline const char* ShadowCube_Fragment() { return GeneratedShaders::OpenGL::shadow_cube_frag; }

// UI shaders
inline const char* RoundedRect_Vertex() { return GeneratedShaders::OpenGL::rounded_rect_vert; }
inline const char* RoundedRect_Fragment() { return GeneratedShaders::OpenGL::rounded_rect_frag; }

inline const char* RoundedRectBorder_Vertex() { return GeneratedShaders::OpenGL::rounded_rect_border_vert; }
inline const char* RoundedRectBorder_Fragment() { return GeneratedShaders::OpenGL::rounded_rect_border_frag; }

// Skybox shaders
inline const char* Skybox_Vertex() { return GeneratedShaders::OpenGL::skybox_vert; }
inline const char* Skybox_Fragment() { return GeneratedShaders::OpenGL::skybox_frag; }

} // namespace DefaultShaders
} // namespace lupine

