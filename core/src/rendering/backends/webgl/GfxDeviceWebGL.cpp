#ifdef __EMSCRIPTEN__

#include "lupine/rendering/backends/GfxDeviceWebGL.hpp"
#include "lupine/rendering/backends/webgl/WebGLState.hpp"
#include "lupine/rendering/backends/webgl/GfxCommandListWebGL.hpp"
#include "lupine/logger/Logger.hpp"
#include "lupine/platform/FileSystem.hpp"
#include "lupine/platform/PackFile.hpp"
#include "lupine/asset/Asset.hpp"
#include "lupine/rendering/FontBaker.hpp"

#include <GLES3/gl3.h>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

#include <algorithm>
#include <unordered_map>
#include <cstring>
#include <vector>

// For stb_truetype font rendering
#include <stb_truetype.h>

namespace lupine {

// Global pointer to the WebGL device for Emscripten callbacks
static GfxDeviceWebGL* g_webglDevice = nullptr;

namespace {

// WebGL/ANGLE rejects any draw whose program has two samplers of DIFFERENT types
// (e.g. sampler2D and samplerCube) resolving to the same texture unit, with
// "GL_INVALID_OPERATION: Two textures of different types use the same sampler location."
// Desktop GL drivers tolerate it. Every sampler uniform defaults to unit 0, so a program
// that mixes sampler types fails the moment the renderer leaves one of them unassigned
// (e.g. the skybox shader's samplerCube + sampler2D pair, only one of which is ever set).
//
// Give every sampler in the program a distinct default unit at link time. The renderer
// still overrides these whenever it binds a texture by name, so this only decides the
// units for samplers that would otherwise all collide on 0.
void assignDefaultSamplerUnits(GLuint program) {
    GLint activeUniforms = 0;
    glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &activeUniforms);
    if (activeUniforms <= 0) {
        return;
    }

    GLint maxTextureUnits = 0;
    glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &maxTextureUnits);
    if (maxTextureUnits <= 0) {
        return;
    }

    GLint nextUnit = 0;
    for (GLint i = 0; i < activeUniforms && nextUnit < maxTextureUnits; ++i) {
        GLint arraySize = 0;
        GLenum type = 0;
        GLsizei nameLength = 0;
        char nameBuf[256] = {};
        glGetActiveUniform(program, static_cast<GLuint>(i), sizeof(nameBuf),
                           &nameLength, &arraySize, &type, nameBuf);

        if (!WebGLUtils::isSamplerUniformType(type)) {
            continue;
        }

        GLint location = glGetUniformLocation(program, nameBuf);
        if (location < 0) {
            continue;
        }

        // Array samplers ("u_ShadowMaps[0]") occupy `arraySize` consecutive units.
        const GLint count = std::min(std::max(arraySize, 1), maxTextureUnits - nextUnit);
        std::vector<GLint> units(static_cast<size_t>(count));
        for (GLint u = 0; u < count; ++u) {
            units[static_cast<size_t>(u)] = nextUnit + u;
        }

        glUniform1iv(location, count, units.data());
        nextUnit += count;
    }
}

} // namespace

struct GfxDeviceWebGL::Impl {
    WebGLState state;
    GfxDeviceCaps caps;
    bool initialized = false;

    FilterMode defaultMinFilter = FilterMode::Linear;
    FilterMode defaultMagFilter = FilterMode::Linear;

    std::unordered_map<uint32_t, GPUMesh> meshes;
    uint32_t nextMeshID = 1;

    std::unordered_map<uint32_t, FontAtlas> fonts;
    std::unordered_map<uint32_t, FontDesc> fontDescs;
    uint32_t nextFontID = 1;

    // Emscripten WebGL context
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE webglContext = 0;
};

GfxDeviceWebGL::GfxDeviceWebGL()
    : m_impl(std::make_unique<Impl>()) {
}

GfxDeviceWebGL::~GfxDeviceWebGL() {
    if (m_impl->initialized) {
        shutdown();
    }
}

bool GfxDeviceWebGL::initializeWebGLContext() {
    EmscriptenWebGLContextAttributes attrs;
    emscripten_webgl_init_context_attributes(&attrs);

    // Request WebGL 2.0 context (OpenGL ES 3.0)
    attrs.majorVersion = 2;
    attrs.minorVersion = 0;
    attrs.alpha = true;
    attrs.depth = true;
    attrs.stencil = true;
    attrs.antialias = true;
    attrs.premultipliedAlpha = true;
    // preserveDrawingBuffer: true helps with context loss recovery
    // but slightly impacts performance. Worth it for robustness.
    attrs.preserveDrawingBuffer = true;
    // Don't fail on software rendering - allows fallback
    attrs.failIfMajorPerformanceCaveat = false;
    attrs.enableExtensionsByDefault = true;
    // Request high-performance GPU to avoid power saving context loss
    attrs.powerPreference = EM_WEBGL_POWER_PREFERENCE_HIGH_PERFORMANCE;

    // Create context on the canvas
    m_impl->webglContext = emscripten_webgl_create_context("#canvas", &attrs);

    if (m_impl->webglContext <= 0) {
        LOG_ERROR(LogCategory::Render, "Failed to create WebGL 2.0 context, trying WebGL 1.0");

        // Fallback to WebGL 1.0
        attrs.majorVersion = 1;
        attrs.minorVersion = 0;
        m_impl->webglContext = emscripten_webgl_create_context("#canvas", &attrs);

        if (m_impl->webglContext <= 0) {
            LOG_ERROR(LogCategory::Render, "Failed to create WebGL context");
            return false;
        }

    }

    // Make context current
    EMSCRIPTEN_RESULT result = emscripten_webgl_make_context_current(m_impl->webglContext);
    if (result != EMSCRIPTEN_RESULT_SUCCESS) {
        LOG_ERROR(LogCategory::Render, "Failed to make WebGL context current");
        emscripten_webgl_destroy_context(m_impl->webglContext);
        return false;
    }

    m_impl->state.contextInitialized = true;
    m_impl->state.contextLost = false;

    // Register context loss callbacks
    registerContextLossCallbacks();

    return true;
}

// Emscripten callback for WebGL context loss
static EM_BOOL onWebGLContextLostCallback(int eventType, const void* reserved, void* userData) {
    (void)eventType;
    (void)reserved;
    LOG_ERROR(LogCategory::Render, "WebGL: Context lost event received");
    if (g_webglDevice) {
        g_webglDevice->onContextLost();
    }
    return EM_TRUE;  // Prevent default behavior, allowing potential recovery
}

// Emscripten callback for WebGL context restoration
static EM_BOOL onWebGLContextRestoredCallback(int eventType, const void* reserved, void* userData) {
    (void)eventType;
    (void)reserved;
    
    if (g_webglDevice) {
        g_webglDevice->onContextRestored();
    }
    return EM_TRUE;
}

void GfxDeviceWebGL::registerContextLossCallbacks() {
    // Store global reference for callbacks
    g_webglDevice = this;

    // Register Emscripten callbacks for context loss/restore events
    // These are triggered by the browser when GPU resources need to be released/recreated
    emscripten_set_webglcontextlost_callback("#canvas", nullptr, EM_FALSE, onWebGLContextLostCallback);
    emscripten_set_webglcontextrestored_callback("#canvas", nullptr, EM_FALSE, onWebGLContextRestoredCallback);

}

void GfxDeviceWebGL::onContextLost() {
    LOG_ERROR(LogCategory::Render, "WebGL: Handling context loss...");

    m_impl->state.contextLost = true;
    m_impl->state.contextLossCount++;
    m_impl->state.contextInitialized = false;

    // Reset all binding state since GL objects are now invalid
    m_impl->state.resetBindingState();

    // Mark all resources as potentially invalid
    // They will need to be recreated when context is restored
    invalidateAllResources();

    LOG_ERROR(LogCategory::Render, "WebGL: Context loss handled (loss count: {})", m_impl->state.contextLossCount);
}

void GfxDeviceWebGL::onContextRestored() {

    // Try to recreate the context
    if (tryRecoverContext()) {
        
    } else {
        LOG_ERROR(LogCategory::Render, "WebGL: Failed to restore context");
    }
}

void GfxDeviceWebGL::invalidateAllResources() {
    // Clear all resource maps - the GL handles are now invalid
    // The engine will need to recreate resources on next use

    // Note: We don't call glDelete* here because the GL context is invalid
    // The browser has already released all GL resources

    m_impl->state.buffers.clear();
    m_impl->state.textures.clear();
    m_impl->state.samplers.clear();
    m_impl->state.shaders.clear();
    m_impl->state.pipelines.clear();
    m_impl->state.renderTargets.clear();
    m_impl->state.uniformBuffers.clear();

    // Clear swapchains except we keep the metadata (size, format)
    // The default framebuffer (0) is automatically recreated by the browser
    for (auto& [id, swapchain] : m_impl->state.swapchains) {
        swapchain.defaultFramebuffer = 0;
    }

    // Clear meshes and fonts
    m_impl->meshes.clear();
    m_impl->fonts.clear();

    // Reset ID counters
    m_impl->state.nextBufferID = 1;
    m_impl->state.nextTextureID = 1;
    m_impl->state.nextSamplerID = 1;
    m_impl->state.nextShaderID = 1;
    m_impl->state.nextPipelineID = 1;
    m_impl->state.nextRenderTargetID = 1;
    m_impl->state.nextUniformBufferID = 1;
    m_impl->nextMeshID = 1;
    m_impl->nextFontID = 1;

    LOG_WARN(LogCategory::Render, "WebGL: All GPU resources invalidated");
}

bool GfxDeviceWebGL::isContextValid() const {
    if (!m_impl) return false;
    if (m_impl->state.contextLost) return false;
    if (!m_impl->state.contextInitialized) return false;
    if (m_impl->webglContext <= 0) return false;

    // Additional check: query if context is still valid
    // This is a quick GL call to verify the context is truly alive
    GLenum err = glGetError();  // Clear any pending errors
    (void)err;  // Ignore the error itself

    // Try a minimal GL operation to verify context
    GLint currentProgram = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);
    err = glGetError();

    // GL_CONTEXT_LOST is 0x0507 in WebGL 2.0
    if (err == 0x0507) {
        LOG_WARN(LogCategory::Render, "WebGL: Context validity check detected lost context");
        // Cast away const to update state - this is intentional for caching
        const_cast<GfxDeviceWebGL*>(this)->m_impl->state.contextLost = true;
        return false;
    }

    return true;
}

bool GfxDeviceWebGL::tryRecoverContext() {

    // Mark as not lost so we can try to use it
    m_impl->state.contextLost = false;

    // Try to make the existing context current
    if (m_impl->webglContext > 0) {
        EMSCRIPTEN_RESULT result = emscripten_webgl_make_context_current(m_impl->webglContext);
        if (result == EMSCRIPTEN_RESULT_SUCCESS) {
            // Query capabilities again in case they changed
            queryCapabilities();
            m_impl->state.contextInitialized = true;
            
            return true;
        }

        // Old context is dead, destroy it
        LOG_WARN(LogCategory::Render, "WebGL: Old context unusable, attempting to create new context");
        emscripten_webgl_destroy_context(m_impl->webglContext);
        m_impl->webglContext = 0;
    }

    // Try to create a new context
    if (initializeWebGLContext()) {
        queryCapabilities();
        
        return true;
    }

    LOG_ERROR(LogCategory::Render, "WebGL: Context recovery failed");
    m_impl->state.contextLost = true;
    return false;
}

void GfxDeviceWebGL::queryCapabilities() {
    m_impl->caps.backend = GraphicsBackend::WebGL;

    // Get WebGL info
    const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));

    m_impl->caps.deviceName = renderer ? renderer : "Unknown WebGL Renderer";
    m_impl->caps.apiVersion = version ? version : "Unknown";

    // Query limits
    GLint maxTextureSize;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
    m_impl->caps.maxTextureSize = static_cast<uint32_t>(maxTextureSize);

    GLint maxArrayTextureLayers;
    glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &maxArrayTextureLayers);
    m_impl->caps.maxTextureLayers = static_cast<uint32_t>(maxArrayTextureLayers);

    GLint maxVertexAttribs;
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxVertexAttribs);
    m_impl->caps.maxVertexAttributes = static_cast<uint32_t>(maxVertexAttribs);

    GLint maxUniformBlockSize;
    glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &maxUniformBlockSize);
    m_impl->caps.maxUniformBufferSize = static_cast<uint32_t>(maxUniformBlockSize);

    // WebGL 2.0 capabilities
    m_impl->caps.supportsCompute = false;           // No compute shaders in WebGL 2.0
    m_impl->caps.supportsGeometryShader = false;    // No geometry shaders
    m_impl->caps.supportsTessellation = false;      // No tessellation
    m_impl->caps.supportsBindless = false;          // No bindless textures
    m_impl->caps.supportsRayTracing = false;        // No ray tracing

    // Check for WebGL extensions
    const char* extensionsStr = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
    std::string extensions = extensionsStr ? extensionsStr : "";

    // Check for anisotropic filtering extension
    // WebGL uses EXT_texture_filter_anisotropic (same as desktop GL)
    if (extensions.find("EXT_texture_filter_anisotropic") != std::string::npos ||
        extensions.find("WEBKIT_EXT_texture_filter_anisotropic") != std::string::npos ||
        extensions.find("MOZ_EXT_texture_filter_anisotropic") != std::string::npos) {
        m_impl->state.supportsAnisotropicFiltering = true;

        // Query max anisotropy value
        // GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT = 0x84FF
        const GLenum GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT = 0x84FF;
        GLfloat maxAniso = 1.0f;
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso);
        m_impl->state.maxAnisotropy = maxAniso;

    } else {
        m_impl->state.supportsAnisotropicFiltering = false;
        m_impl->state.maxAnisotropy = 1.0f;
        
    }

    // Check for debug extension
    if (extensions.find("WEBGL_debug") != std::string::npos ||
        extensions.find("WEBGL_debug_shaders") != std::string::npos ||
        extensions.find("WEBGL_debug_renderer_info") != std::string::npos) {
        m_impl->state.supportsDebugOutput = true;

    } else {
        m_impl->state.supportsDebugOutput = false;
    }

    // Check for EXT_color_buffer_float extension (required for rendering to float textures)
    if (extensions.find("EXT_color_buffer_float") != std::string::npos) {
        m_impl->state.supportsColorBufferFloat = true;
        
    } else {
        m_impl->state.supportsColorBufferFloat = false;
        LOG_WARN(LogCategory::Render, "WebGL: EXT_color_buffer_float not available, float render targets may not work");
    }

}

bool GfxDeviceWebGL::initialize() {
    if (m_impl->initialized) {
        return true;
    }

    // WebGL context will be created when the first swapchain is created
    // This is because we need the canvas element to exist first

    m_impl->initialized = true;

    return true;
}

void GfxDeviceWebGL::shutdown() {
    // Clear global pointer
    if (g_webglDevice == this) {
        g_webglDevice = nullptr;
    }

    if (!m_impl->initialized) {
        return;
    }

    // Destroy all meshes
    for (auto& [id, mesh] : m_impl->meshes) {
        destroyBuffer(mesh.vertexBuffer);
        destroyBuffer(mesh.indexBuffer);
    }
    m_impl->meshes.clear();

    // Destroy all fonts
    for (auto& [id, font] : m_impl->fonts) {
        destroyTexture(font.texture);
    }
    m_impl->fonts.clear();

    // Destroy all buffers
    for (auto& [id, buffer] : m_impl->state.buffers) {
        glDeleteBuffers(1, &buffer.id);
    }
    m_impl->state.buffers.clear();

    // Destroy all textures
    for (auto& [id, texture] : m_impl->state.textures) {
        glDeleteTextures(1, &texture.id);
    }
    m_impl->state.textures.clear();

    // Destroy all samplers
    for (auto& [id, sampler] : m_impl->state.samplers) {
        glDeleteSamplers(1, &sampler.id);
    }
    m_impl->state.samplers.clear();

    // Destroy all shaders
    for (auto& [id, shader] : m_impl->state.shaders) {
        glDeleteShader(shader.id);
    }
    m_impl->state.shaders.clear();

    // Destroy all pipelines
    for (auto& [id, pipeline] : m_impl->state.pipelines) {
        glDeleteProgram(pipeline.program);
        if (pipeline.vao != 0) {
            glDeleteVertexArrays(1, &pipeline.vao);
        }
    }
    m_impl->state.pipelines.clear();

    // Destroy all render targets
    for (auto& [id, rt] : m_impl->state.renderTargets) {
        if (rt.fbo != 0) {
            glDeleteFramebuffers(1, &rt.fbo);
        }
        if (rt.colorTexture != 0) {
            glDeleteTextures(1, &rt.colorTexture);
        }
        if (rt.depthStencilTexture != 0) {
            glDeleteTextures(1, &rt.depthStencilTexture);
        }
        if (rt.depthRenderbuffer != 0) {
            glDeleteRenderbuffers(1, &rt.depthRenderbuffer);
        }
    }
    m_impl->state.renderTargets.clear();

    // Destroy all uniform buffers
    for (auto& [id, ubo] : m_impl->state.uniformBuffers) {
        glDeleteBuffers(1, &ubo.id);
    }
    m_impl->state.uniformBuffers.clear();

    // Destroy WebGL context
    if (m_impl->webglContext > 0) {
        emscripten_webgl_destroy_context(m_impl->webglContext);
        m_impl->webglContext = 0;
    }

    m_impl->state.contextInitialized = false;
    m_impl->initialized = false;
}

const GfxDeviceCaps& GfxDeviceWebGL::getCapabilities() const {
    return m_impl->caps;
}

GraphicsBackend GfxDeviceWebGL::getBackend() const {
    return GraphicsBackend::WebGL;
}

void GfxDeviceWebGL::setDefaultTextureFiltering(FilterMode minFilter, FilterMode magFilter) {
    m_impl->defaultMinFilter = minFilter;
    m_impl->defaultMagFilter = magFilter;
}

// =============================================================================
// Swapchain Management
// =============================================================================

SwapchainHandle GfxDeviceWebGL::createSwapchain(const SwapchainDesc& desc) {
    // Initialize WebGL context if not done yet
    if (!m_impl->state.contextInitialized) {
        if (!initializeWebGLContext()) {
            return SwapchainHandle();
        }
        queryCapabilities();
    }

    // Get canvas size
    int canvasWidth = desc.width;
    int canvasHeight = desc.height;

    if (canvasWidth == 0 || canvasHeight == 0) {
        // Query canvas size from Emscripten
        double cssWidth, cssHeight;
        emscripten_get_element_css_size("#canvas", &cssWidth, &cssHeight);
        canvasWidth = static_cast<int>(cssWidth);
        canvasHeight = static_cast<int>(cssHeight);

        if (canvasWidth == 0 || canvasHeight == 0) {
            canvasWidth = 800;
            canvasHeight = 600;
            
        }
    }

    // Set canvas size
    emscripten_set_canvas_element_size("#canvas", canvasWidth, canvasHeight);

    WebGLSwapchain swapchain;
    swapchain.width = canvasWidth;
    swapchain.height = canvasHeight;
    swapchain.colorFormat = desc.colorFormat;
    swapchain.vsync = desc.vsync;
    swapchain.defaultFramebuffer = 0; // WebGL canvas uses FBO 0
    swapchain.canvasId = "#canvas";

    // Create backbuffer render target (FBO 0)
    WebGLRenderTarget backbuffer;
    backbuffer.fbo = 0;
    backbuffer.width = canvasWidth;
    backbuffer.height = canvasHeight;
    backbuffer.colorFormat = desc.colorFormat;
    backbuffer.isSwapchainBackbuffer = true;

    uint32_t rtID = m_impl->state.nextRenderTargetID++;
    m_impl->state.renderTargets[rtID] = backbuffer;
    swapchain.backbuffer = RenderTargetHandle(rtID);

    uint32_t id = m_impl->state.nextSwapchainID++;
    m_impl->state.swapchains[id] = swapchain;

    return SwapchainHandle(id);
}

void GfxDeviceWebGL::destroySwapchain(SwapchainHandle swapchain) {
    auto it = m_impl->state.swapchains.find(swapchain.id);
    if (it == m_impl->state.swapchains.end()) {
        return;
    }

    // Destroy backbuffer render target
    RenderTargetHandle backbuffer = it->second.backbuffer;
    if (backbuffer.isValid()) {
        m_impl->state.renderTargets.erase(backbuffer.id);
    }

    m_impl->state.swapchains.erase(it);
}

void GfxDeviceWebGL::resizeSwapchain(SwapchainHandle swapchain, uint32_t width, uint32_t height) {
    auto it = m_impl->state.swapchains.find(swapchain.id);
    if (it == m_impl->state.swapchains.end()) {
        return;
    }

    it->second.width = width;
    it->second.height = height;

    // Update canvas size
    emscripten_set_canvas_element_size(it->second.canvasId.c_str(), width, height);

    // Update backbuffer dimensions
    auto rtIt = m_impl->state.renderTargets.find(it->second.backbuffer.id);
    if (rtIt != m_impl->state.renderTargets.end()) {
        rtIt->second.width = width;
        rtIt->second.height = height;
    }

}

void GfxDeviceWebGL::present(SwapchainHandle swapchain) {
    // Skip if context is lost
    if (m_impl->state.contextLost) {
        return;
    }

    // WebGL automatically presents when the JavaScript event loop returns
    // No explicit swap buffers needed
    glFlush();

    // Check for context loss after present
    GLenum err = glGetError();
    if (err == 0x0507) {  // GL_CONTEXT_LOST
        LOG_ERROR(LogCategory::Render, "WebGL: Context lost detected during present");
        m_impl->state.contextLost = true;
    }
}

RenderTargetHandle GfxDeviceWebGL::getSwapchainBackbuffer(SwapchainHandle swapchain) {
    auto it = m_impl->state.swapchains.find(swapchain.id);
    if (it != m_impl->state.swapchains.end()) {
        return it->second.backbuffer;
    }
    return RenderTargetHandle();
}

void GfxDeviceWebGL::makeContextCurrent(SwapchainHandle /*swapchain*/) {
    // WebGL has a single context that's always current.
    // Context is managed by the browser and Emscripten.
    // This method exists for API compatibility with OpenGL-style backends.
}

void GfxDeviceWebGL::setSwapchainHintForOffscreen(SwapchainHandle /*swapchain*/) {
    // No-op for WebGL - single context, no synchronization requirements
}

// =============================================================================
// Texture Management
// =============================================================================

TextureHandle GfxDeviceWebGL::createTexture(const TextureDesc& desc) {
    // Check context validity before creating resources
    if (m_impl->state.contextLost) {
        LOG_WARN(LogCategory::Render, "WebGL: Cannot create texture - context lost");
        return TextureHandle();
    }

    // Check format support and get fallback if needed
    TextureFormat actualFormat = desc.format;
    if (!WebGLUtils::isFormatSupported(desc.format)) {
        actualFormat = WebGLUtils::getFallbackFormat(desc.format);
        
    }

    WebGLTexture texture;
    texture.width = desc.width;
    texture.height = desc.height;
    texture.depth = desc.depth;
    texture.mipLevels = desc.mipLevels;
    texture.format = actualFormat;
    texture.target = WebGLUtils::toWebGLTextureTarget(desc.type);

    glGenTextures(1, &texture.id);

    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        LOG_ERROR(LogCategory::Render, "glGenTextures failed with error: {}", err);
        return TextureHandle();
    }

    glBindTexture(texture.target, texture.id);

    GLenum internalFormat = WebGLUtils::toWebGLInternalFormat(actualFormat);
    GLenum format = WebGLUtils::toWebGLTextureFormat(actualFormat);
    GLenum dataType = WebGLUtils::toWebGLDataType(actualFormat);

    if (texture.target == GL_TEXTURE_2D) {
        if (desc.width == 0 || desc.height == 0) {
            LOG_ERROR(LogCategory::Render, "Invalid texture dimensions: {}x{}", desc.width, desc.height);
            glDeleteTextures(1, &texture.id);
            return TextureHandle();
        }

        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, desc.width, desc.height,
                     0, format, dataType, desc.initialData);

        err = glGetError();
        if (err != GL_NO_ERROR) {
            LOG_ERROR(LogCategory::Render, "glTexImage2D failed with error: {}", err);
            glDeleteTextures(1, &texture.id);
            return TextureHandle();
        }
    } else if (texture.target == GL_TEXTURE_3D) {
        glTexImage3D(GL_TEXTURE_3D, 0, internalFormat, desc.width, desc.height, desc.depth,
                     0, format, dataType, desc.initialData);
    } else if (texture.target == GL_TEXTURE_CUBE_MAP) {
        for (int face = 0; face < 6; ++face) {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, internalFormat,
                        desc.width, desc.height, 0, format, dataType, nullptr);
        }
    } else if (texture.target == GL_TEXTURE_2D_ARRAY) {
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, internalFormat, desc.width, desc.height, desc.arrayLayers,
                     0, format, dataType, desc.initialData);
    }

    // Generate mipmaps if requested
    if (desc.mipLevels > 1) {
        glGenerateMipmap(texture.target);
    }

    // Set default filtering
    GLenum minFilter = (m_impl->defaultMinFilter == FilterMode::Nearest) ? GL_NEAREST : GL_LINEAR;
    GLenum magFilter = (m_impl->defaultMagFilter == FilterMode::Nearest) ? GL_NEAREST : GL_LINEAR;

    if (desc.mipLevels > 1) {
        minFilter = (m_impl->defaultMinFilter == FilterMode::Nearest) ?
                    GL_NEAREST_MIPMAP_NEAREST : GL_LINEAR_MIPMAP_LINEAR;
    }

    glTexParameteri(texture.target, GL_TEXTURE_MIN_FILTER, minFilter);
    glTexParameteri(texture.target, GL_TEXTURE_MAG_FILTER, magFilter);
    glTexParameteri(texture.target, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(texture.target, GL_TEXTURE_WRAP_T, GL_REPEAT);

    if (texture.target == GL_TEXTURE_3D || texture.target == GL_TEXTURE_CUBE_MAP) {
        glTexParameteri(texture.target, GL_TEXTURE_WRAP_R, GL_REPEAT);
    }

    glBindTexture(texture.target, 0);

    uint32_t id = m_impl->state.nextTextureID++;
    m_impl->state.textures[id] = texture;

    // DEBUG: Log texture creation
    static int createLogCount = 0;
    if (createLogCount < 5) {
        
        createLogCount++;
    }

    return TextureHandle(id);
}

void GfxDeviceWebGL::destroyTexture(TextureHandle texture) {
    auto it = m_impl->state.textures.find(texture.id);
    if (it != m_impl->state.textures.end()) {
        GLuint glTextureId = it->second.id;
        glDeleteTextures(1, &glTextureId);

        // Clear any stale bindings
        for (size_t i = 0; i < m_impl->state.boundTextures.size(); ++i) {
            if (m_impl->state.boundTextures[i].id == glTextureId) {
                m_impl->state.boundTextures[i].id = 0;
                m_impl->state.boundTextures[i].target = 0;
            }
        }

        m_impl->state.textures.erase(it);
    }
}

void GfxDeviceWebGL::updateTexture(TextureHandle texture, const void* data,
                                    uint32_t mipLevel, uint32_t arrayLayer) {
    auto it = m_impl->state.textures.find(texture.id);
    if (it == m_impl->state.textures.end() || !data) {
        return;
    }

    const WebGLTexture& glTexture = it->second;
    glBindTexture(glTexture.target, glTexture.id);

    GLenum format = WebGLUtils::toWebGLTextureFormat(glTexture.format);
    GLenum dataType = WebGLUtils::toWebGLDataType(glTexture.format);

    if (glTexture.target == GL_TEXTURE_2D) {
        glTexSubImage2D(GL_TEXTURE_2D, mipLevel, 0, 0,
                        glTexture.width, glTexture.height,
                        format, dataType, data);
    } else if (glTexture.target == GL_TEXTURE_2D_ARRAY) {
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, mipLevel, 0, 0, arrayLayer,
                        glTexture.width, glTexture.height, 1,
                        format, dataType, data);
    } else if (glTexture.target == GL_TEXTURE_CUBE_MAP) {
        glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + arrayLayer, mipLevel, 0, 0,
                        glTexture.width, glTexture.height,
                        format, dataType, data);
    }

    glBindTexture(glTexture.target, 0);
}

// =============================================================================
// Buffer Management
// =============================================================================

BufferHandle GfxDeviceWebGL::createBuffer(const BufferDesc& desc) {
    // Check context validity before creating resources
    if (m_impl->state.contextLost) {
        LOG_WARN(LogCategory::Render, "WebGL: Cannot create buffer - context lost");
        return BufferHandle();
    }

    WebGLBuffer buffer;
    buffer.size = desc.size;
    buffer.usage = desc.usage;

    // Determine buffer target
    if ((static_cast<uint32_t>(desc.usage) & static_cast<uint32_t>(BufferUsage::Vertex)) != 0) {
        buffer.target = GL_ARRAY_BUFFER;
    } else if ((static_cast<uint32_t>(desc.usage) & static_cast<uint32_t>(BufferUsage::Index)) != 0) {
        buffer.target = GL_ELEMENT_ARRAY_BUFFER;
    } else if ((static_cast<uint32_t>(desc.usage) & static_cast<uint32_t>(BufferUsage::Uniform)) != 0) {
        buffer.target = GL_UNIFORM_BUFFER;
    } else {
        buffer.target = GL_ARRAY_BUFFER;
    }

    glGenBuffers(1, &buffer.id);
    glBindBuffer(buffer.target, buffer.id);
    glBufferData(buffer.target, desc.size, desc.initialData, GL_STATIC_DRAW);
    glBindBuffer(buffer.target, 0);

    uint32_t id = m_impl->state.nextBufferID++;
    m_impl->state.buffers[id] = buffer;

    return BufferHandle(id);
}

void GfxDeviceWebGL::destroyBuffer(BufferHandle buffer) {
    auto it = m_impl->state.buffers.find(buffer.id);
    if (it != m_impl->state.buffers.end()) {
        glDeleteBuffers(1, &it->second.id);
        m_impl->state.buffers.erase(it);
    }
}

void GfxDeviceWebGL::updateBuffer(BufferHandle buffer, const void* data,
                                   uint64_t size, uint64_t offset) {
    auto it = m_impl->state.buffers.find(buffer.id);
    if (it == m_impl->state.buffers.end() || !data) {
        return;
    }

    const WebGLBuffer& glBuffer = it->second;

    glBindBuffer(glBuffer.target, glBuffer.id);
    glBufferSubData(glBuffer.target, offset, size, data);
    glBindBuffer(glBuffer.target, 0);
}

// =============================================================================
// Sampler Management
// =============================================================================

SamplerHandle GfxDeviceWebGL::createSampler(const SamplerDesc& desc) {
    WebGLSampler sampler;
    sampler.desc = desc;

    glGenSamplers(1, &sampler.id);

    // Set sampler parameters
    glSamplerParameteri(sampler.id, GL_TEXTURE_MIN_FILTER, WebGLUtils::toWebGLFilterMode(desc.minFilter));
    glSamplerParameteri(sampler.id, GL_TEXTURE_MAG_FILTER, WebGLUtils::toWebGLFilterMode(desc.magFilter));
    glSamplerParameteri(sampler.id, GL_TEXTURE_WRAP_S, WebGLUtils::toWebGLWrapMode(desc.wrapU));
    glSamplerParameteri(sampler.id, GL_TEXTURE_WRAP_T, WebGLUtils::toWebGLWrapMode(desc.wrapV));
    glSamplerParameteri(sampler.id, GL_TEXTURE_WRAP_R, WebGLUtils::toWebGLWrapMode(desc.wrapW));

    // Apply anisotropic filtering if extension is available
    if (m_impl->state.supportsAnisotropicFiltering && desc.maxAnisotropy > 1.0f) {
        // GL_TEXTURE_MAX_ANISOTROPY_EXT = 0x84FE
        const GLenum GL_TEXTURE_MAX_ANISOTROPY_EXT = 0x84FE;
        float anisotropy = std::min(desc.maxAnisotropy, m_impl->state.maxAnisotropy);
        glSamplerParameterf(sampler.id, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropy);
    }

    if (desc.compareEnable) {
        glSamplerParameteri(sampler.id, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glSamplerParameteri(sampler.id, GL_TEXTURE_COMPARE_FUNC, WebGLUtils::toWebGLCompareFunc(desc.compareFunc));
    }

    uint32_t id = m_impl->state.nextSamplerID++;
    m_impl->state.samplers[id] = sampler;

    return SamplerHandle(id);
}

void GfxDeviceWebGL::destroySampler(SamplerHandle sampler) {
    auto it = m_impl->state.samplers.find(sampler.id);
    if (it != m_impl->state.samplers.end()) {
        glDeleteSamplers(1, &it->second.id);
        m_impl->state.samplers.erase(it);
    }
}

// =============================================================================
// Shader Management
// =============================================================================

ShaderHandle GfxDeviceWebGL::createShader(const ShaderDesc& desc) {
    // Check context validity before creating resources
    if (m_impl->state.contextLost) {
        LOG_WARN(LogCategory::Render, "WebGL: Cannot create shader - context lost");
        return ShaderHandle();
    }

    // WebGL doesn't support geometry or compute shaders
    if (desc.stage == ShaderStage::Geometry || desc.stage == ShaderStage::Compute) {
        LOG_ERROR(LogCategory::Render, "Shader stage not supported in WebGL 2.0");
        return ShaderHandle();
    }

    WebGLShader shader;
    shader.stage = desc.stage;

    GLenum shaderType = WebGLUtils::toWebGLShaderStage(desc.stage);
    shader.id = glCreateShader(shaderType);

    const char* source = static_cast<const char*>(desc.bytecode);
    GLint length = static_cast<GLint>(desc.bytecodeSize);

    glShaderSource(shader.id, 1, &source, &length);
    glCompileShader(shader.id);

    GLint success;
    glGetShaderiv(shader.id, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLint logLength = 0;
        glGetShaderiv(shader.id, GL_INFO_LOG_LENGTH, &logLength);

        std::string errorMsg = "(no error message available)";
        if (logLength > 0) {
            std::vector<char> infoLog(logLength + 1);
            glGetShaderInfoLog(shader.id, logLength, nullptr, infoLog.data());
            errorMsg = infoLog.data();
        }

        // Log the first 200 chars of shader source for debugging
        std::string srcPreview(source, std::min(static_cast<size_t>(length), size_t(200)));
        LOG_ERROR(LogCategory::Render, "Shader compilation failed: {}", errorMsg);
        LOG_ERROR(LogCategory::Render, "Shader source (first 200 chars): {}", srcPreview);

        glDeleteShader(shader.id);
        return ShaderHandle();
    }

    uint32_t id = m_impl->state.nextShaderID++;
    m_impl->state.shaders[id] = shader;

    return ShaderHandle(id);
}

void GfxDeviceWebGL::destroyShader(ShaderHandle shader) {
    auto it = m_impl->state.shaders.find(shader.id);
    if (it != m_impl->state.shaders.end()) {
        glDeleteShader(it->second.id);
        m_impl->state.shaders.erase(it);
    }
}

// =============================================================================
// Pipeline Management
// =============================================================================

PipelineHandle GfxDeviceWebGL::createPipeline(const PipelineDesc& desc) {
    // Check context validity before creating resources
    if (m_impl->state.contextLost) {
        LOG_WARN(LogCategory::Render, "WebGL: Cannot create pipeline - context lost");
        return PipelineHandle();
    }

    WebGLPipeline pipeline;
    pipeline.shaders = desc.shaders;
    pipeline.vertexLayout = desc.vertexLayout;
    pipeline.extraVertexBuffers = desc.extraVertexBuffers;
    pipeline.topology = desc.topology;
    pipeline.blendState = desc.blendState;
    pipeline.depthStencilState = desc.depthStencilState;
    pipeline.rasterizerState = desc.rasterizerState;

    // Create shader program
    pipeline.program = glCreateProgram();

    // Attach shaders
    for (const ShaderHandle& shaderHandle : desc.shaders) {
        auto it = m_impl->state.shaders.find(shaderHandle.id);
        if (it != m_impl->state.shaders.end()) {
            glAttachShader(pipeline.program, it->second.id);
        }
    }

    // Link program
    glLinkProgram(pipeline.program);

    GLint success;
    glGetProgramiv(pipeline.program, GL_LINK_STATUS, &success);
    if (!success) {
        GLint logLength;
        glGetProgramiv(pipeline.program, GL_INFO_LOG_LENGTH, &logLength);

        std::vector<char> infoLog(logLength + 1);
        glGetProgramInfoLog(pipeline.program, logLength, nullptr, infoLog.data());

        LOG_ERROR(LogCategory::Render, "Shader program linking failed: {}", infoLog.data());

        glDeleteProgram(pipeline.program);
        return PipelineHandle();
    }

    // Detach shaders (they can be deleted now)
    for (const ShaderHandle& shaderHandle : desc.shaders) {
        auto it = m_impl->state.shaders.find(shaderHandle.id);
        if (it != m_impl->state.shaders.end()) {
            glDetachShader(pipeline.program, it->second.id);
        }
    }

    // Create VAO for this pipeline
    glGenVertexArrays(1, &pipeline.vao);
    pipeline.vaoInitialized = false;

    // Cache common uniform locations
    glUseProgram(pipeline.program);

    assignDefaultSamplerUnits(pipeline.program);

    // Set up uniform block bindings
    GLuint lightDataBlockIndex = glGetUniformBlockIndex(pipeline.program, "LightData");
    if (lightDataBlockIndex != GL_INVALID_INDEX) {
        glUniformBlockBinding(pipeline.program, lightDataBlockIndex, 3);
    }

    // Cache uniform locations
    pipeline.uniformLocations["u_ViewProjection"] = glGetUniformLocation(pipeline.program, "u_ViewProjection");
    pipeline.uniformLocations["u_Model"] = glGetUniformLocation(pipeline.program, "u_Model");
    pipeline.uniformLocations["u_NormalMatrix"] = glGetUniformLocation(pipeline.program, "u_NormalMatrix");
    pipeline.uniformLocations["u_TintColor"] = glGetUniformLocation(pipeline.program, "u_TintColor");
    pipeline.uniformLocations["u_Texture"] = glGetUniformLocation(pipeline.program, "u_Texture");
    pipeline.uniformLocations["u_UseTexture"] = glGetUniformLocation(pipeline.program, "u_UseTexture");
    pipeline.uniformLocations["u_UVRect"] = glGetUniformLocation(pipeline.program, "u_UVRect");
    pipeline.uniformLocations["u_FontAtlas"] = glGetUniformLocation(pipeline.program, "u_FontAtlas");

    glUseProgram(0);

    uint32_t id = m_impl->state.nextPipelineID++;
    m_impl->state.pipelines[id] = pipeline;

    return PipelineHandle(id);
}

void GfxDeviceWebGL::destroyPipeline(PipelineHandle pipeline) {
    auto it = m_impl->state.pipelines.find(pipeline.id);
    if (it != m_impl->state.pipelines.end()) {
        glDeleteProgram(it->second.program);
        if (it->second.vao != 0) {
            glDeleteVertexArrays(1, &it->second.vao);
        }
        m_impl->state.pipelines.erase(it);
    }
}

// =============================================================================
// Render Target Management
// =============================================================================

RenderTargetHandle GfxDeviceWebGL::createRenderTarget(const RenderTargetDesc& desc) {
    // Check context validity before creating resources
    if (m_impl->state.contextLost) {
        LOG_WARN(LogCategory::Render, "WebGL: Cannot create render target - context lost");
        return RenderTargetHandle();
    }

    // Create a mutable copy of desc for format fallbacks
    RenderTargetDesc actualDesc = desc;

    // Apply format fallbacks when EXT_color_buffer_float is not available
    if (!m_impl->state.supportsColorBufferFloat) {
        // Fall back float color formats to RGBA8
        if (actualDesc.hasColor) {
            switch (actualDesc.colorFormat) {
                case TextureFormat::R16_FLOAT:
                case TextureFormat::R32_FLOAT:
                    actualDesc.colorFormat = TextureFormat::R8_UNORM;
                    LOG_WARN(LogCategory::Render, "WebGL: Float color format not supported, falling back to R8_UNORM");
                    break;
                case TextureFormat::RG16_FLOAT:
                case TextureFormat::RG32_FLOAT:
                    actualDesc.colorFormat = TextureFormat::RG8_UNORM;
                    LOG_WARN(LogCategory::Render, "WebGL: Float color format not supported, falling back to RG8_UNORM");
                    break;
                case TextureFormat::RGBA16_FLOAT:
                case TextureFormat::RGBA32_FLOAT:
                    actualDesc.colorFormat = TextureFormat::RGBA8_UNORM;
                    LOG_WARN(LogCategory::Render, "WebGL: Float color format not supported, falling back to RGBA8_UNORM");
                    break;
                default:
                    break;
            }
        }

        // Fall back DEPTH32F to DEPTH24
        if (actualDesc.hasDepth) {
            if (actualDesc.depthFormat == TextureFormat::DEPTH32F) {
                actualDesc.depthFormat = TextureFormat::DEPTH24;
                LOG_WARN(LogCategory::Render, "WebGL: DEPTH32F not supported for framebuffers, falling back to DEPTH24");
            } else if (actualDesc.depthFormat == TextureFormat::DEPTH32F_STENCIL8) {
                actualDesc.depthFormat = TextureFormat::DEPTH24_STENCIL8;
                LOG_WARN(LogCategory::Render, "WebGL: DEPTH32F_STENCIL8 not supported, falling back to DEPTH24_STENCIL8");
            }
        }
    }

    WebGLRenderTarget renderTarget;
    renderTarget.width = actualDesc.width;
    renderTarget.height = actualDesc.height;
    renderTarget.colorFormat = actualDesc.colorFormat;
    renderTarget.depthFormat = actualDesc.depthFormat;
    renderTarget.isCubeMap = actualDesc.isCubeMap;
    renderTarget.currentCubeFace = actualDesc.cubeMapFace;

    glGenFramebuffers(1, &renderTarget.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, renderTarget.fbo);

    // Create color attachment
    if (actualDesc.hasColor) {
        GLenum colorTarget = actualDesc.isCubeMap ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D;

        glGenTextures(1, &renderTarget.colorTexture);
        glBindTexture(colorTarget, renderTarget.colorTexture);

        GLenum colorInternalFormat = WebGLUtils::toWebGLInternalFormat(actualDesc.colorFormat);
        GLenum colorFormat = WebGLUtils::toWebGLTextureFormat(actualDesc.colorFormat);
        GLenum colorType = WebGLUtils::toWebGLDataType(actualDesc.colorFormat);

        if (actualDesc.isCubeMap) {
            for (int face = 0; face < 6; ++face) {
                glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, colorInternalFormat,
                            actualDesc.width, actualDesc.height, 0, colorFormat, colorType, nullptr);
            }
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

            if (actualDesc.cubeMapFace >= 0 && actualDesc.cubeMapFace < 6) {
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                      GL_TEXTURE_CUBE_MAP_POSITIVE_X + actualDesc.cubeMapFace,
                                      renderTarget.colorTexture, 0);
            }
        } else {
            glTexImage2D(GL_TEXTURE_2D, 0, colorInternalFormat, actualDesc.width, actualDesc.height,
                        0, colorFormat, colorType, nullptr);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                  GL_TEXTURE_2D, renderTarget.colorTexture, 0);
        }

        // Register color texture
        WebGLTexture colorTex;
        colorTex.id = renderTarget.colorTexture;
        colorTex.width = actualDesc.width;
        colorTex.height = actualDesc.height;
        colorTex.format = actualDesc.colorFormat;
        colorTex.target = colorTarget;

        uint32_t colorTexID = m_impl->state.nextTextureID++;
        m_impl->state.textures[colorTexID] = colorTex;
        renderTarget.colorTextureHandle = TextureHandle(colorTexID);
    } else {
        // No color attachment - depth-only render target
        GLenum drawBuffers[] = { GL_NONE };
        glDrawBuffers(1, drawBuffers);
    }

    // Create depth attachment
    if (actualDesc.hasDepth) {
        GLenum depthTarget = actualDesc.isCubeMap ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D;

        glGenTextures(1, &renderTarget.depthStencilTexture);
        glBindTexture(depthTarget, renderTarget.depthStencilTexture);

        GLenum depthInternalFormat = WebGLUtils::toWebGLInternalFormat(actualDesc.depthFormat);
        GLenum depthFormat = WebGLUtils::toWebGLTextureFormat(actualDesc.depthFormat);
        GLenum depthType = WebGLUtils::toWebGLDataType(actualDesc.depthFormat);

        if (actualDesc.isCubeMap) {
            for (int face = 0; face < 6; ++face) {
                glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, depthInternalFormat,
                            actualDesc.width, actualDesc.height, 0, depthFormat, depthType, nullptr);
            }
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        } else {
            glTexImage2D(GL_TEXTURE_2D, 0, depthInternalFormat, actualDesc.width, actualDesc.height,
                        0, depthFormat, depthType, nullptr);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }

        GLenum attachment = (actualDesc.depthFormat == TextureFormat::DEPTH24_STENCIL8 ||
                            actualDesc.depthFormat == TextureFormat::DEPTH32F_STENCIL8) ?
                            GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT;

        if (actualDesc.isCubeMap && actualDesc.cubeMapFace >= 0 && actualDesc.cubeMapFace < 6) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, attachment,
                                  GL_TEXTURE_CUBE_MAP_POSITIVE_X + actualDesc.cubeMapFace,
                                  renderTarget.depthStencilTexture, 0);
        } else if (!actualDesc.isCubeMap) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, attachment,
                                  GL_TEXTURE_2D, renderTarget.depthStencilTexture, 0);
        }

        // Register depth texture
        WebGLTexture depthTex;
        depthTex.id = renderTarget.depthStencilTexture;
        depthTex.width = actualDesc.width;
        depthTex.height = actualDesc.height;
        depthTex.format = actualDesc.depthFormat;
        depthTex.target = depthTarget;

        uint32_t depthTexID = m_impl->state.nextTextureID++;
        m_impl->state.textures[depthTexID] = depthTex;
        renderTarget.depthTextureHandle = TextureHandle(depthTexID);
    }

    // Check framebuffer completeness
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        LOG_ERROR(LogCategory::Render, "Framebuffer is not complete: 0x{:x}", status);

        glDeleteFramebuffers(1, &renderTarget.fbo);
        if (renderTarget.colorTexture != 0) {
            glDeleteTextures(1, &renderTarget.colorTexture);
        }
        if (renderTarget.depthStencilTexture != 0) {
            glDeleteTextures(1, &renderTarget.depthStencilTexture);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return RenderTargetHandle();
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    uint32_t id = m_impl->state.nextRenderTargetID++;
    m_impl->state.renderTargets[id] = renderTarget;

    return RenderTargetHandle(id);
}

void GfxDeviceWebGL::destroyRenderTarget(RenderTargetHandle target) {
    auto it = m_impl->state.renderTargets.find(target.id);
    if (it != m_impl->state.renderTargets.end()) {
        const WebGLRenderTarget& rt = it->second;

        if (!rt.isSwapchainBackbuffer) {
            if (rt.fbo != 0) {
                glDeleteFramebuffers(1, &rt.fbo);
            }
            if (rt.colorTexture != 0) {
                glDeleteTextures(1, &rt.colorTexture);
                if (rt.colorTextureHandle.isValid()) {
                    m_impl->state.textures.erase(rt.colorTextureHandle.id);
                }
            }
            if (rt.depthStencilTexture != 0) {
                glDeleteTextures(1, &rt.depthStencilTexture);
                if (rt.depthTextureHandle.isValid()) {
                    m_impl->state.textures.erase(rt.depthTextureHandle.id);
                }
            }
            if (rt.depthRenderbuffer != 0) {
                glDeleteRenderbuffers(1, &rt.depthRenderbuffer);
            }
        }

        m_impl->state.renderTargets.erase(it);
    }
}

TextureHandle GfxDeviceWebGL::getRenderTargetColorTexture(RenderTargetHandle target) {
    auto it = m_impl->state.renderTargets.find(target.id);
    if (it != m_impl->state.renderTargets.end()) {
        return it->second.colorTextureHandle;
    }
    return TextureHandle();
}

TextureHandle GfxDeviceWebGL::getRenderTargetDepthTexture(RenderTargetHandle target) {
    auto it = m_impl->state.renderTargets.find(target.id);
    if (it != m_impl->state.renderTargets.end()) {
        return it->second.depthTextureHandle;
    }
    return TextureHandle();
}

void GfxDeviceWebGL::attachCubeMapFace(RenderTargetHandle target, uint32_t faceIndex) {
    if (faceIndex > 5) {
        
        return;
    }

    auto it = m_impl->state.renderTargets.find(target.id);
    if (it == m_impl->state.renderTargets.end()) {
        return;
    }

    WebGLRenderTarget& rt = it->second;
    if (!rt.isCubeMap) {
        
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, rt.fbo);

    if (rt.colorTexture != 0) {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_TEXTURE_CUBE_MAP_POSITIVE_X + faceIndex,
                              rt.colorTexture, 0);
    }

    if (rt.depthStencilTexture != 0) {
        GLenum attachment = (rt.depthFormat == TextureFormat::DEPTH24_STENCIL8 ||
                            rt.depthFormat == TextureFormat::DEPTH32F_STENCIL8) ?
                            GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT;

        glFramebufferTexture2D(GL_FRAMEBUFFER, attachment,
                              GL_TEXTURE_CUBE_MAP_POSITIVE_X + faceIndex,
                              rt.depthStencilTexture, 0);
    }

    rt.currentCubeFace = faceIndex;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GfxDeviceWebGL::unbindFramebuffer() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    m_impl->state.boundFramebuffer = 0;
}

// =============================================================================
// Uniform Buffer Management
// =============================================================================

UniformBufferHandle GfxDeviceWebGL::createUniformBuffer(uint32_t size) {
    WebGLUniformBuffer ubo;
    ubo.size = size;

    glGenBuffers(1, &ubo.id);
    glBindBuffer(GL_UNIFORM_BUFFER, ubo.id);
    glBufferData(GL_UNIFORM_BUFFER, size, nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    uint32_t id = m_impl->state.nextUniformBufferID++;
    m_impl->state.uniformBuffers[id] = ubo;

    return UniformBufferHandle(id);
}

void GfxDeviceWebGL::destroyUniformBuffer(UniformBufferHandle buffer) {
    auto it = m_impl->state.uniformBuffers.find(buffer.id);
    if (it != m_impl->state.uniformBuffers.end()) {
        glDeleteBuffers(1, &it->second.id);
        m_impl->state.uniformBuffers.erase(it);
    }
}

void GfxDeviceWebGL::updateUniformBuffer(UniformBufferHandle buffer, const void* data,
                                          uint32_t size, uint32_t offset) {
    auto it = m_impl->state.uniformBuffers.find(buffer.id);
    if (it == m_impl->state.uniformBuffers.end() || !data) {
        return;
    }

    const WebGLUniformBuffer& ubo = it->second;
    glBindBuffer(GL_UNIFORM_BUFFER, ubo.id);
    glBufferSubData(GL_UNIFORM_BUFFER, offset, size, data);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

// =============================================================================
// Command Recording & Execution
// =============================================================================

std::unique_ptr<IGfxCommandList> GfxDeviceWebGL::beginFrame(RenderTargetHandle target) {
    // Check if context is valid - if not, try to recover
    if (m_impl->state.contextLost) {
        LOG_WARN(LogCategory::Render, "WebGL: beginFrame called with lost context, attempting recovery...");
        if (!tryRecoverContext()) {
            LOG_ERROR(LogCategory::Render, "WebGL: Cannot begin frame - context lost and recovery failed");
            return nullptr;
        }
    }

    // Ensure WebGL context is current
    if (m_impl->webglContext > 0) {
        EMSCRIPTEN_RESULT result = emscripten_webgl_make_context_current(m_impl->webglContext);
        if (result != EMSCRIPTEN_RESULT_SUCCESS) {
            LOG_ERROR(LogCategory::Render, "WebGL: Failed to make context current in beginFrame");
            m_impl->state.contextLost = true;
            return nullptr;
        }
    } else {
        LOG_ERROR(LogCategory::Render, "WebGL: No valid context in beginFrame");
        return nullptr;
    }

    // Verify context is truly valid with a quick GL call
    GLenum err = glGetError();  // Clear pending errors
    if (err == 0x0507) {  // GL_CONTEXT_LOST
        LOG_ERROR(LogCategory::Render, "WebGL: Context lost detected in beginFrame");
        m_impl->state.contextLost = true;
        return nullptr;
    }

    auto cmd = std::make_unique<GfxCommandListWebGL>(&m_impl->state);
    cmd->setRenderTarget(target);

    return cmd;
}

void GfxDeviceWebGL::submit(std::unique_ptr<IGfxCommandList> commandList) {
    // Skip if context is lost
    if (m_impl->state.contextLost) {
        return;
    }

    if (auto* webglCmd = dynamic_cast<GfxCommandListWebGL*>(commandList.get())) {
        webglCmd->execute();

        // Check for context loss after execution
        GLenum err = glGetError();
        if (err == 0x0507) {  // GL_CONTEXT_LOST
            LOG_ERROR(LogCategory::Render, "WebGL: Context lost detected during submit");
            m_impl->state.contextLost = true;
        }
    }
}

void GfxDeviceWebGL::waitIdle() {
    if (m_impl->state.contextLost) {
        return;
    }
    glFinish();
}

// =============================================================================
// Mesh Management
// =============================================================================

MeshHandle GfxDeviceWebGL::createMesh(const MeshData& meshData) {
    GPUMesh gpuMesh;

    // Create vertex buffer
    BufferDesc vbDesc;
    vbDesc.size = meshData.vertices.size() * sizeof(Vertex);
    vbDesc.usage = BufferUsage::Vertex;
    vbDesc.initialData = meshData.vertices.data();

    gpuMesh.vertexBuffer = createBuffer(vbDesc);
    gpuMesh.vertexCount = static_cast<uint32_t>(meshData.vertices.size());

    // Create index buffer
    BufferDesc ibDesc;
    ibDesc.size = meshData.indices.size() * sizeof(uint32_t);
    ibDesc.usage = BufferUsage::Index;
    ibDesc.initialData = meshData.indices.data();

    gpuMesh.indexBuffer = createBuffer(ibDesc);
    gpuMesh.indexCount = static_cast<uint32_t>(meshData.indices.size());

    gpuMesh.submeshes = meshData.submeshes;
    gpuMesh.bounds = meshData.bounds;

    uint32_t id = m_impl->nextMeshID++;
    m_impl->meshes[id] = gpuMesh;

    return MeshHandle(id);
}

const GPUMesh* GfxDeviceWebGL::getMesh(MeshHandle handle) const {
    auto it = m_impl->meshes.find(handle.id);
    if (it != m_impl->meshes.end()) {
        return &it->second;
    }
    return nullptr;
}

void GfxDeviceWebGL::destroyMesh(MeshHandle handle) {
    auto it = m_impl->meshes.find(handle.id);
    if (it != m_impl->meshes.end()) {
        destroyBuffer(it->second.vertexBuffer);
        destroyBuffer(it->second.indexBuffer);
        m_impl->meshes.erase(it);
    }
}

// =============================================================================
// Font Management
// =============================================================================

FontAtlas GfxDeviceWebGL::buildBakedAtlas(const BakedFontAtlas& baked) {
    FontAtlas fontAtlas;
    if (!baked.success) {
        return fontAtlas;
    }

    TextureDesc texDesc;
    texDesc.width = baked.atlasWidth;
    texDesc.height = baked.atlasHeight;
    texDesc.format = TextureFormat::R8_UNORM;
    texDesc.mipLevels = 1;
    texDesc.usage = TextureUsage::Sampled;
    texDesc.initialData = baked.bitmap.data();

    TextureHandle atlasTexture = createTexture(texDesc);
    if (!atlasTexture.isValid()) {
        return fontAtlas;
    }

    fontAtlas.texture = atlasTexture;
    fontAtlas.atlasWidth = baked.atlasWidth;
    fontAtlas.atlasHeight = baked.atlasHeight;
    fontAtlas.fontSize = baked.fontSize;
    fontAtlas.lineHeight = baked.lineHeight;
    fontAtlas.ascent = baked.ascent;
    fontAtlas.descent = baked.descent;
    fontAtlas.glyphs = baked.glyphs;
    fontAtlas.kerning = baked.kerning;
    return fontAtlas;
}

FontHandle GfxDeviceWebGL::createFontAtlas(const FontDesc& desc) {
    BakedFontAtlas baked = BakeFontAtlas(desc, GetFontOversample());
    if (!baked.success) {
        LOG_ERROR(LogCategory::Render, "Failed to bake font: {}", desc.fontPath);
        return FontHandle();
    }

    FontAtlas fontAtlas = buildBakedAtlas(baked);
    if (!fontAtlas.texture.isValid()) {
        return FontHandle();
    }

    uint32_t fontID = m_impl->nextFontID++;
    m_impl->fonts[fontID] = std::move(fontAtlas);
    m_impl->fontDescs[fontID] = desc;

    return FontHandle(fontID);
}

void GfxDeviceWebGL::refreshFontAtlases() {
    const float oversample = GetFontOversample();
    for (const auto& [fontID, desc] : m_impl->fontDescs) {
        BakedFontAtlas baked = BakeFontAtlas(desc, oversample);
        FontAtlas fontAtlas = buildBakedAtlas(baked);
        if (!fontAtlas.texture.isValid()) {
            continue;
        }

        auto it = m_impl->fonts.find(fontID);
        if (it != m_impl->fonts.end() && it->second.texture.isValid()) {
            destroyTexture(it->second.texture);
        }
        m_impl->fonts[fontID] = std::move(fontAtlas);
    }
}

const FontAtlas* GfxDeviceWebGL::getFontAtlas(FontHandle handle) const {
    if (!handle.isValid()) {
        return nullptr;
    }

    auto it = m_impl->fonts.find(handle.id);
    if (it != m_impl->fonts.end()) {
        return &it->second;
    }
    return nullptr;
}

void GfxDeviceWebGL::destroyFontAtlas(FontHandle handle) {
    if (!handle.isValid()) {
        return;
    }

    auto it = m_impl->fonts.find(handle.id);
    if (it != m_impl->fonts.end()) {
        destroyTexture(it->second.texture);
        m_impl->fonts.erase(it);
    }
    m_impl->fontDescs.erase(handle.id);
}

// =============================================================================
// Debug Renderer Factory (stub for WebGL - debug rendering not yet implemented)
// =============================================================================

} // namespace lupine

// Factory function for debug renderer - must be outside namespace for proper linkage
#include "lupine/rendering/debug/DebugRenderer.hpp"

namespace lupine {

std::unique_ptr<DebugRenderer> createDebugRenderer(GraphicsBackend backend) {
    // WebGL debug renderer not yet implemented - return nullptr
    // Debug visualization will be disabled but engine will function
    (void)backend;
    return nullptr;
}

} // namespace lupine

#endif // __EMSCRIPTEN__
