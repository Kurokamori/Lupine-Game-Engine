#pragma once

#ifdef __EMSCRIPTEN__

#include "../../ResourceHandles.hpp"
#include "../../gfx/GfxTypes.hpp"
#include "../../gfx/GfxDescriptors.hpp"
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <string>

// WebGL types (using Emscripten's OpenGL ES 3.0 bindings)
typedef unsigned int GLuint;
typedef unsigned int GLenum;
typedef int GLint;
typedef int GLsizei;
typedef unsigned char GLboolean;
typedef float GLfloat;

namespace lupine {

/**
 * WebGL resource structures
 * Similar to OpenGL but adapted for WebGL 2.0 (OpenGL ES 3.0)
 */

struct WebGLBuffer {
    GLuint id = 0;
    uint64_t size = 0;
    BufferUsage usage = BufferUsage::None;
    GLenum target = 0; // GL_ARRAY_BUFFER, GL_ELEMENT_ARRAY_BUFFER, etc.
};

struct WebGLTexture {
    GLuint id = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t depth = 1;
    uint32_t mipLevels = 1;
    TextureFormat format = TextureFormat::Unknown;
    GLenum target = 0; // GL_TEXTURE_2D, GL_TEXTURE_CUBE_MAP, etc.
};

struct WebGLSampler {
    GLuint id = 0;
    SamplerDesc desc;
};

struct WebGLShader {
    GLuint id = 0;
    ShaderStage stage;
};

struct WebGLPipeline {
    GLuint program = 0;
    GLuint vao = 0; // Single VAO per pipeline (WebGL has single context)
    bool vaoInitialized = false;

    std::vector<ShaderHandle> shaders;
    VertexBufferLayout vertexLayout;
    // Additional per-binding layouts (e.g. binding 1 = per-instance data). Used
    // to set up instanced vertex attributes with glVertexAttribDivisor.
    std::vector<VertexBufferLayout> extraVertexBuffers;
    PrimitiveTopology topology = PrimitiveTopology::TriangleList;
    BlendState blendState;
    DepthStencilState depthStencilState;
    RasterizerState rasterizerState;

    // Uniform locations cache
    std::unordered_map<std::string, GLint> uniformLocations;
};

struct WebGLRenderTarget {
    GLuint fbo = 0; // Framebuffer object
    GLuint colorTexture = 0;
    GLuint depthStencilTexture = 0;
    GLuint depthRenderbuffer = 0; // For MSAA or when texture is not needed
    uint32_t width = 0;
    uint32_t height = 0;
    TextureFormat colorFormat = TextureFormat::Unknown;
    TextureFormat depthFormat = TextureFormat::Unknown;
    bool isSwapchainBackbuffer = false;
    bool isCubeMap = false;
    int currentCubeFace = -1;

    // Handles to the textures
    TextureHandle colorTextureHandle;
    TextureHandle depthTextureHandle;
};

struct WebGLSwapchain {
    // WebGL uses the canvas as the primary render surface
    // No platform window handle needed - just the canvas element
    uint32_t width = 0;
    uint32_t height = 0;
    TextureFormat colorFormat = TextureFormat::Unknown;
    bool vsync = true;

    // Default framebuffer (0 for WebGL canvas)
    GLuint defaultFramebuffer = 0;

    // Associated render target for the backbuffer
    RenderTargetHandle backbuffer;

    // Canvas element ID (for Emscripten)
    std::string canvasId = "#canvas";
};

struct WebGLUniformBuffer {
    GLuint id = 0;
    uint32_t size = 0;
};

/**
 * WebGL state manager.
 * Manages all WebGL resources and state tracking for a single context.
 */
class WebGLState {
public:
    WebGLState();
    ~WebGLState();

    // Resource storage
    std::unordered_map<uint32_t, WebGLBuffer> buffers;
    std::unordered_map<uint32_t, WebGLTexture> textures;
    std::unordered_map<uint32_t, WebGLSampler> samplers;
    std::unordered_map<uint32_t, WebGLShader> shaders;
    std::unordered_map<uint32_t, WebGLPipeline> pipelines;
    std::unordered_map<uint32_t, WebGLRenderTarget> renderTargets;
    std::unordered_map<uint32_t, WebGLSwapchain> swapchains;
    std::unordered_map<uint32_t, WebGLUniformBuffer> uniformBuffers;

    // Resource ID generators
    uint32_t nextBufferID = 1;
    uint32_t nextTextureID = 1;
    uint32_t nextSamplerID = 1;
    uint32_t nextShaderID = 1;
    uint32_t nextPipelineID = 1;
    uint32_t nextRenderTargetID = 1;
    uint32_t nextSwapchainID = 1;
    uint32_t nextUniformBufferID = 1;

    // State tracking (for minimizing state changes)
    GLuint boundProgram = 0;
    GLuint boundVAO = 0;
    GLuint boundArrayBuffer = 0;
    GLuint boundElementBuffer = 0;
    GLuint boundFramebuffer = 0;

    // Texture binding state - track both ID and target per unit
    struct TextureBinding {
        GLuint id = 0;
        GLenum target = 0;
    };
    std::vector<TextureBinding> boundTextures; // Per texture unit
    std::vector<GLuint> boundSamplers; // Per sampler unit

    // Viewport and scissor state
    struct ViewportState {
        float x = 0, y = 0, width = 0, height = 0;
        float minDepth = 0, maxDepth = 1;
    } viewport;

    struct ScissorState {
        int32_t x = 0, y = 0;
        uint32_t width = 0, height = 0;
        bool enabled = false;
    } scissor;

    // WebGL context initialized flag
    bool contextInitialized = false;

    // Context loss tracking - critical for robustness
    bool contextLost = false;
    int contextLossCount = 0;  // Track how many times context has been lost

    // Extension support flags
    bool supportsAnisotropicFiltering = false;
    float maxAnisotropy = 1.0f;
    bool supportsDebugOutput = false;
    bool supportsColorBufferFloat = false;  // EXT_color_buffer_float for rendering to float textures

    // Current framebuffer attachment tracking (to prevent feedback loops)
    // When we bind a framebuffer, we track which GL texture IDs are attached to it.
    // Before draw calls, we can check if any bound textures would create a feedback loop.
    GLuint currentFboColorTexture = 0;
    GLuint currentFboDepthTexture = 0;

    // Helper methods for state caching
    void bindProgram(GLuint program);
    void bindVAO(GLuint vao);
    void bindBuffer(GLenum target, GLuint buffer);
    void bindFramebuffer(GLuint fbo);
    void bindTexture(uint32_t unit, GLenum target, GLuint texture);
    void bindSampler(uint32_t unit, GLuint sampler);

    // Unbind any textures that are attached to the current framebuffer
    // Call this before draw calls to prevent feedback loops
    void unbindFramebufferTextures();

    // Reset all binding state
    void resetBindingState();
};

/**
 * WebGL helper functions
 * Conversion utilities for engine types to WebGL/OpenGL ES types
 */
namespace WebGLUtils {
    // Convert engine types to WebGL types
    GLenum toWebGLTextureFormat(TextureFormat format);
    GLenum toWebGLInternalFormat(TextureFormat format);
    GLenum toWebGLDataType(TextureFormat format);
    GLenum toWebGLTextureTarget(TextureType type);
    GLenum toWebGLTopology(PrimitiveTopology topology);
    GLenum toWebGLCompareFunc(CompareFunc func);
    GLenum toWebGLBlendFactor(BlendFactor factor);
    GLenum toWebGLBlendOp(BlendOp op);
    GLenum toWebGLFilterMode(FilterMode mode);
    GLenum toWebGLWrapMode(WrapMode mode);
    GLenum toWebGLShaderStage(ShaderStage stage);
    GLuint getVertexFormatSize(VertexFormat format);
    GLenum getVertexFormatType(VertexFormat format);
    GLint getVertexFormatCount(VertexFormat format);
    bool isVertexFormatNormalized(VertexFormat format);

    // True for every GLSL ES 3.0 sampler uniform type (sampler2D, samplerCube,
    // sampler2DShadow, isampler*, usampler*, ...).
    bool isSamplerUniformType(GLenum uniformType);

    // Check if a texture format is supported in WebGL 2.0
    bool isFormatSupported(TextureFormat format);

    // Get appropriate fallback format for unsupported formats
    TextureFormat getFallbackFormat(TextureFormat format);
}

} // namespace lupine

#endif // __EMSCRIPTEN__
