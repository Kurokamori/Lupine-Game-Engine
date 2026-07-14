#pragma once

#include "../../ResourceHandles.hpp"
#include "../../gfx/GfxTypes.hpp"
#include "../../gfx/GfxDescriptors.hpp"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cstdint>

// Forward declare OpenGL types to avoid including GL headers here
typedef unsigned int GLuint;
typedef unsigned int GLenum;
typedef int GLint;

namespace lupine {

/**
 * OpenGL resource structures
 */

struct GLBuffer {
    GLuint id = 0;
    uint64_t size = 0;
    BufferUsage usage = BufferUsage::None;
    GLenum target = 0; // GL_ARRAY_BUFFER, GL_ELEMENT_ARRAY_BUFFER, etc.
    void* ownerContext = nullptr; // Context that created this buffer (nullptr = shared)
};

struct GLTexture {
    GLuint id = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t depth = 1;
    uint32_t mipLevels = 1;
    TextureFormat format = TextureFormat::Unknown;
    GLenum target = 0; // GL_TEXTURE_2D, GL_TEXTURE_CUBE_MAP, etc.
    void* ownerContext = nullptr; // Context that created this texture (nullptr = shared)
};

struct GLSampler {
    GLuint id = 0;
    SamplerDesc desc;
};

struct GLShader {
    GLuint id = 0;
    ShaderStage stage;
};

struct GLPipeline {
    GLuint program = 0;

    // VAOs are NOT shared between contexts, so we need one per context
    // Key is the context handle (HGLRC on Windows)
    mutable std::unordered_map<void*, GLuint> vaos;

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

    // Track if VAO has been initialized with a vertex buffer per context
    // This is mutable because it's modified during rendering, not during pipeline creation
    mutable std::unordered_map<void*, bool> vaoInitialized;
};

struct GLRenderTarget {
    GLuint fbo = 0; // Framebuffer object
    GLuint colorTexture = 0;
    GLuint depthStencilTexture = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    TextureFormat colorFormat = TextureFormat::Unknown;
    TextureFormat depthFormat = TextureFormat::Unknown;
    bool isSwapchainBackbuffer = false;
    bool isCubeMap = false;
    int currentCubeFace = -1; // Currently attached cube face (-1 = none/all)

    // Handles to the textures (so we can return them via getRenderTargetColorTexture/DepthTexture)
    TextureHandle colorTextureHandle;
    TextureHandle depthTextureHandle;
};

struct GLSwapchain {
    NativeWindowHandle window;
    uint32_t width = 0;
    uint32_t height = 0;
    TextureFormat colorFormat = TextureFormat::Unknown;
    bool vsync = true;

    // Platform-specific context handle
    void* contextHandle = nullptr;
    void* deviceContext = nullptr;  // HDC on Windows

    // Default framebuffer (0 for main window)
    GLuint defaultFramebuffer = 0;

    // Associated render target
    RenderTargetHandle backbuffer;
};

struct GLUniformBuffer {
    GLuint id = 0;
    uint32_t size = 0;
};

/**
 * OpenGL state manager.
 * Manages all OpenGL resources and state tracking.
 */
class OpenGLState {
public:
    OpenGLState();
    ~OpenGLState();

    // Resource storage
    std::unordered_map<uint32_t, GLBuffer> buffers;
    std::unordered_map<uint32_t, GLTexture> textures;
    std::unordered_map<uint32_t, GLSampler> samplers;
    std::unordered_map<uint32_t, GLShader> shaders;
    std::unordered_map<uint32_t, GLPipeline> pipelines;
    std::unordered_map<uint32_t, GLRenderTarget> renderTargets;
    std::unordered_map<uint32_t, GLSwapchain> swapchains;
    std::unordered_map<uint32_t, GLUniformBuffer> uniformBuffers;

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

    // Track current context to detect context switches
    void* currentContext = nullptr;
    
    // Track all created contexts for resource management
    std::unordered_set<void*> allContexts;
    
    // Track which contexts are isolated (don't share resources)
    std::unordered_set<void*> isolatedContexts;

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

    // Helper methods
    void bindProgram(GLuint program);
    void bindVAO(GLuint vao);
    void bindBuffer(GLenum target, GLuint buffer);
    void bindFramebuffer(GLuint fbo);
    void bindTexture(uint32_t unit, GLenum target, GLuint texture);
    void bindSampler(uint32_t unit, GLuint sampler);

    // Reset all binding state (call when runtime restarts to avoid stale texture ID issues)
    void resetBindingState();
};

/**
 * OpenGL helper functions
 */
namespace GLUtils {
    // Convert engine types to OpenGL types
    GLenum toGLTextureFormat(TextureFormat format);
    GLenum toGLInternalFormat(TextureFormat format);
    GLenum toGLDataType(TextureFormat format);
    GLenum toGLTextureTarget(TextureType type);
    GLenum toGLTopology(PrimitiveTopology topology);
    GLenum toGLCompareFunc(CompareFunc func);
    GLenum toGLBlendFactor(BlendFactor factor);
    GLenum toGLBlendOp(BlendOp op);
    GLenum toGLFilterMode(FilterMode mode);
    GLenum toGLWrapMode(WrapMode mode);
    GLenum toGLShaderStage(ShaderStage stage);
    GLuint getVertexFormatSize(VertexFormat format);
    GLenum getVertexFormatType(VertexFormat format);
    GLint getVertexFormatCount(VertexFormat format);
    bool isVertexFormatNormalized(VertexFormat format);
}

} // namespace lupine
