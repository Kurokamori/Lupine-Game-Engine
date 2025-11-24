#include "lupine/rendering/backends/GfxDeviceOpenGL.hpp"
#include "lupine/rendering/backends/opengl/OpenGLState.hpp"
#include "lupine/rendering/backends/opengl/GfxCommandListOpenGL.hpp"
#include "lupine/logger/Logger.hpp"
#include <GL/glew.h>
#include <GL/wglew.h>
#include <unordered_map>
#include <cstring>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace lupine {

struct GfxDeviceOpenGL::Impl {
    OpenGLState state;
    GfxDeviceCaps caps;
    bool initialized = false;

    FilterMode defaultMinFilter = FilterMode::Linear;
    FilterMode defaultMagFilter = FilterMode::Linear;

    std::unordered_map<uint32_t, GPUMesh> meshes;
    uint32_t nextMeshID = 1;

    std::unordered_map<uint32_t, FontAtlas> fonts;
    uint32_t nextFontID = 1;
};

GfxDeviceOpenGL::GfxDeviceOpenGL()
    : m_impl(std::make_unique<Impl>()) {
}

GfxDeviceOpenGL::~GfxDeviceOpenGL() {
    if (m_impl->initialized) {
        shutdown();
    }
}

bool GfxDeviceOpenGL::initialize() {
    if (m_impl->initialized) {
        return true;
    }

    m_impl->initialized = true;

    return true;
}

bool GfxDeviceOpenGL::ensureGLEWInitialized() {
    static bool glewInitialized = false;

    if (glewInitialized) {
        return true;
    }

    GLenum err = glewInit();
    if (err != GLEW_OK) {

        return false;
    }

    m_impl->caps.backend = GraphicsBackend::OpenGL;
    m_impl->caps.deviceName = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    m_impl->caps.apiVersion = reinterpret_cast<const char*>(glGetString(GL_VERSION));

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

    m_impl->caps.supportsCompute = GLEW_ARB_compute_shader || GLEW_VERSION_4_3;
    m_impl->caps.supportsGeometryShader = GLEW_VERSION_3_2;
    m_impl->caps.supportsTessellation = GLEW_ARB_tessellation_shader || GLEW_VERSION_4_0;
    m_impl->caps.supportsBindless = GLEW_ARB_bindless_texture;
    m_impl->caps.supportsRayTracing = false;

    if (GLEW_ARB_debug_output) {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    }

    glewInitialized = true;

    return true;
}

void GfxDeviceOpenGL::setDefaultTextureFiltering(FilterMode minFilter, FilterMode magFilter) {
    m_impl->defaultMinFilter = minFilter;
    m_impl->defaultMagFilter = magFilter;
}

void GfxDeviceOpenGL::shutdown() {
    if (!m_impl->initialized) {
        return;
    }

    for (auto& [id, mesh] : m_impl->meshes) {
        destroyBuffer(mesh.vertexBuffer);
        destroyBuffer(mesh.indexBuffer);
    }
    m_impl->meshes.clear();

    for (auto& [name, font] : m_impl->fonts) {
        destroyTexture(font.texture);
    }
    m_impl->fonts.clear();

    for (auto& [id, buffer] : m_impl->state.buffers) {
        glDeleteBuffers(1, &buffer.id);
    }
    m_impl->state.buffers.clear();

    for (auto& [id, texture] : m_impl->state.textures) {
        glDeleteTextures(1, &texture.id);
    }
    m_impl->state.textures.clear();

    for (auto& [id, sampler] : m_impl->state.samplers) {
        glDeleteSamplers(1, &sampler.id);
    }
    m_impl->state.samplers.clear();

    for (auto& [id, shader] : m_impl->state.shaders) {
        glDeleteShader(shader.id);
    }
    m_impl->state.shaders.clear();

    for (auto& [id, pipeline] : m_impl->state.pipelines) {
        glDeleteProgram(pipeline.program);

        for (const auto& [context, vao] : pipeline.vaos) {
            glDeleteVertexArrays(1, &vao);
        }
    }
    m_impl->state.pipelines.clear();

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
    }
    m_impl->state.renderTargets.clear();

    for (auto& [id, ubo] : m_impl->state.uniformBuffers) {
        glDeleteBuffers(1, &ubo.id);
    }
    m_impl->state.uniformBuffers.clear();

    m_impl->initialized = false;

}

const GfxDeviceCaps& GfxDeviceOpenGL::getCapabilities() const {
    return m_impl->caps;
}

GraphicsBackend GfxDeviceOpenGL::getBackend() const {
    return GraphicsBackend::OpenGL;
}

SwapchainHandle GfxDeviceOpenGL::createSwapchain(const SwapchainDesc& desc) {
#ifdef _WIN32

    HWND hwnd = static_cast<HWND>(desc.window.platformHandle);
    HDC hdc = GetDC(hwnd);

    if (!hdc) {

        return SwapchainHandle();
    }

    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;

    int pixelFormat = ChoosePixelFormat(hdc, &pfd);
    if (!pixelFormat) {

        ReleaseDC(hwnd, hdc);
        return SwapchainHandle();
    }

    if (!SetPixelFormat(hdc, pixelFormat, &pfd)) {

        ReleaseDC(hwnd, hdc);
        return SwapchainHandle();
    }

    HGLRC hglrc = wglCreateContext(hdc);
    if (!hglrc) {

        ReleaseDC(hwnd, hdc);
        return SwapchainHandle();
    }

    if (!desc.isolated && !m_impl->state.swapchains.empty()) {

        HGLRC shareContext = nullptr;
        for (const auto& [id, swapchain] : m_impl->state.swapchains) {
            if (swapchain.contextHandle) {
                shareContext = static_cast<HGLRC>(swapchain.contextHandle);
                break;
            }
        }

        if (shareContext) {

            wglMakeCurrent(nullptr, nullptr);
            m_impl->state.currentContext = nullptr;

            if (!wglShareLists(shareContext, hglrc)) {
                DWORD error = GetLastError();

                wglDeleteContext(hglrc);
                ReleaseDC(hwnd, hdc);
                return SwapchainHandle();
            }

        }
    } else if (desc.isolated) {

        m_impl->state.isolatedContexts.insert(hglrc);
    }

    if (!wglMakeCurrent(hdc, hglrc)) {

        wglDeleteContext(hglrc);
        ReleaseDC(hwnd, hdc);
        return SwapchainHandle();
    }

    m_impl->state.currentContext = hglrc;
    m_impl->state.allContexts.insert(hglrc);
    m_impl->state.boundProgram = 0;
    m_impl->state.boundVAO = 0;
    m_impl->state.boundArrayBuffer = 0;
    m_impl->state.boundElementBuffer = 0;
    m_impl->state.boundFramebuffer = 0;

    if (!ensureGLEWInitialized()) {

        wglMakeCurrent(nullptr, nullptr);
        m_impl->state.currentContext = nullptr;
        wglDeleteContext(hglrc);
        ReleaseDC(hwnd, hdc);
        return SwapchainHandle();
    }

    if (WGLEW_EXT_swap_control) {
        if (desc.vsync) {

            if (WGLEW_EXT_swap_control_tear) {
                wglSwapIntervalEXT(-1);

            } else {
                wglSwapIntervalEXT(1);

            }
        } else {
            wglSwapIntervalEXT(0);

        }
    } else {

    }

    GLSwapchain swapchain;
    swapchain.window = desc.window;
    swapchain.width = desc.width;
    swapchain.height = desc.height;
    swapchain.colorFormat = desc.colorFormat;
    swapchain.vsync = desc.vsync;
    swapchain.defaultFramebuffer = 0;
    swapchain.contextHandle = hglrc;
    swapchain.deviceContext = hdc;

    GLRenderTarget backbuffer;
    backbuffer.fbo = 0;
    backbuffer.width = desc.width;
    backbuffer.height = desc.height;
    backbuffer.colorFormat = desc.colorFormat;
    backbuffer.isSwapchainBackbuffer = true;

    uint32_t rtID = m_impl->state.nextRenderTargetID++;
    m_impl->state.renderTargets[rtID] = backbuffer;
    swapchain.backbuffer = RenderTargetHandle(rtID);

    uint32_t id = m_impl->state.nextSwapchainID++;
    m_impl->state.swapchains[id] = swapchain;

    return SwapchainHandle(id);
#else

    return SwapchainHandle();
#endif
}

void GfxDeviceOpenGL::destroySwapchain(SwapchainHandle swapchain) {
    auto it = m_impl->state.swapchains.find(swapchain.id);
    if (it == m_impl->state.swapchains.end()) {
        return;
    }

#ifdef _WIN32

    if (it->second.contextHandle) {
        HGLRC hglrc = static_cast<HGLRC>(it->second.contextHandle);
        HDC hdc = static_cast<HDC>(it->second.deviceContext);

        bool isIsolated = m_impl->state.isolatedContexts.count(hglrc) > 0;

        if (hdc) {
            wglMakeCurrent(hdc, hglrc);
            m_impl->state.currentContext = hglrc;
        }

        RenderTargetHandle backbuffer = it->second.backbuffer;
        if (backbuffer.isValid()) {
            auto rtIt = m_impl->state.renderTargets.find(backbuffer.id);
            if (rtIt != m_impl->state.renderTargets.end()) {
                const GLRenderTarget& rt = rtIt->second;

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

                m_impl->state.renderTargets.erase(backbuffer.id);
            }
        }

        m_impl->state.allContexts.erase(hglrc);
        m_impl->state.isolatedContexts.erase(hglrc);

        if (isIsolated) {

            for (auto bufIt = m_impl->state.buffers.begin(); bufIt != m_impl->state.buffers.end(); ) {
                if (bufIt->second.ownerContext == hglrc) {
                    glDeleteBuffers(1, &bufIt->second.id);

                    bufIt = m_impl->state.buffers.erase(bufIt);
                } else {
                    ++bufIt;
                }
            }

            for (auto texIt = m_impl->state.textures.begin(); texIt != m_impl->state.textures.end(); ) {
                if (texIt->second.ownerContext == hglrc) {
                    glDeleteTextures(1, &texIt->second.id);

                    texIt = m_impl->state.textures.erase(texIt);
                } else {
                    ++texIt;
                }
            }

            for (auto& [pipelineId, pipeline] : m_impl->state.pipelines) {
                auto vaoIt = pipeline.vaos.find(hglrc);
                if (vaoIt != pipeline.vaos.end()) {
                    glDeleteVertexArrays(1, &vaoIt->second);
                    pipeline.vaos.erase(vaoIt);
                    pipeline.vaoInitialized.erase(hglrc);
                }
            }
        } else {

        }

        wglMakeCurrent(nullptr, nullptr);
        m_impl->state.currentContext = nullptr;
        wglDeleteContext(hglrc);

        if (hdc && it->second.window.platformHandle) {
            HWND hwnd = static_cast<HWND>(it->second.window.platformHandle);
            ReleaseDC(hwnd, hdc);
        }
    }
#endif

    m_impl->state.swapchains.erase(it);
}

void GfxDeviceOpenGL::resizeSwapchain(SwapchainHandle swapchain, uint32_t width, uint32_t height) {
    auto it = m_impl->state.swapchains.find(swapchain.id);
    if (it == m_impl->state.swapchains.end()) {
        return;
    }

    it->second.width = width;
    it->second.height = height;

    auto rtIt = m_impl->state.renderTargets.find(it->second.backbuffer.id);
    if (rtIt != m_impl->state.renderTargets.end()) {
        rtIt->second.width = width;
        rtIt->second.height = height;
    }
}

void GfxDeviceOpenGL::present(SwapchainHandle swapchain) {
    auto it = m_impl->state.swapchains.find(swapchain.id);
    if (it == m_impl->state.swapchains.end()) {
        return;
    }

#ifdef _WIN32

    if (it->second.deviceContext && it->second.contextHandle) {
        HDC hdc = static_cast<HDC>(it->second.deviceContext);
        HGLRC hglrc = static_cast<HGLRC>(it->second.contextHandle);

        if (m_impl->state.currentContext != hglrc) {
            if (!wglMakeCurrent(hdc, hglrc)) {

                return;
            }

            m_impl->state.currentContext = hglrc;
            m_impl->state.boundProgram = UINT32_MAX;
            m_impl->state.boundVAO = UINT32_MAX;
            m_impl->state.boundArrayBuffer = UINT32_MAX;
            m_impl->state.boundElementBuffer = UINT32_MAX;
            m_impl->state.boundFramebuffer = UINT32_MAX;

            for (auto& binding : m_impl->state.boundTextures) {
                binding.id = UINT32_MAX;
                binding.target = 0;
            }
            for (auto& sampler : m_impl->state.boundSamplers) {
                sampler = 0;
            }

            glDisable(GL_SCISSOR_TEST);
            m_impl->state.scissor = {0, 0, 0, 0, false};
        }

        glFlush();

        SwapBuffers(hdc);
    }
#endif
}

RenderTargetHandle GfxDeviceOpenGL::getSwapchainBackbuffer(SwapchainHandle swapchain) {
    auto it = m_impl->state.swapchains.find(swapchain.id);
    if (it != m_impl->state.swapchains.end()) {
        return it->second.backbuffer;
    }
    return RenderTargetHandle();
}

TextureHandle GfxDeviceOpenGL::createTexture(const TextureDesc& desc) {
    GLTexture texture;
    texture.width = desc.width;
    texture.height = desc.height;
    texture.depth = desc.depth;
    texture.mipLevels = desc.mipLevels;
    texture.format = desc.format;
    texture.target = GLUtils::toGLTextureTarget(desc.type);

    glGenTextures(1, &texture.id);
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        LOG_ERROR(LogCategory::Render, "OpenGL error after glGenTextures: 0x{:X}", err);
    }

    glBindTexture(texture.target, texture.id);
    err = glGetError();
    if (err != GL_NO_ERROR) {
        LOG_ERROR(LogCategory::Render, "OpenGL error after glBindTexture: 0x{:X}", err);
    }

    GLenum internalFormat = GLUtils::toGLInternalFormat(desc.format);
    GLenum format = GLUtils::toGLTextureFormat(desc.format);
    GLenum dataType = GLUtils::toGLDataType(desc.format);

    LOG_INFO(LogCategory::Render, "Creating texture: {}x{}, internalFormat=0x{:X}, format=0x{:X}, dataType=0x{:X}, data={}",
        desc.width, desc.height, internalFormat, format, dataType, (void*)desc.initialData);

    if (texture.target == GL_TEXTURE_2D) {
        // Validate texture dimensions
        if (desc.width == 0 || desc.height == 0) {
            LOG_ERROR(LogCategory::Render, "Invalid texture dimensions: {}x{}", desc.width, desc.height);
            glDeleteTextures(1, &texture.id);
            return TextureHandle();
        }

        // Validate data pointer if we're uploading initial data
        if (desc.initialData != nullptr) {
            LOG_INFO(LogCategory::Render, "Uploading texture data to GPU...");
        } else {
            LOG_INFO(LogCategory::Render, "Creating empty texture (no initial data)");
        }

        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, desc.width, desc.height,
                     0, format, dataType, desc.initialData);
        err = glGetError();
        if (err != GL_NO_ERROR) {
            LOG_ERROR(LogCategory::Render, "OpenGL error after glTexImage2D: 0x{:X}", err);
            glDeleteTextures(1, &texture.id);
            return TextureHandle();
        }
        LOG_INFO(LogCategory::Render, "Texture uploaded successfully");
    } else if (texture.target == GL_TEXTURE_3D) {
        glTexImage3D(GL_TEXTURE_3D, 0, internalFormat, desc.width, desc.height, desc.depth,
                     0, format, dataType, desc.initialData);
        err = glGetError();
        if (err != GL_NO_ERROR) {
            LOG_ERROR(LogCategory::Render, "OpenGL error after glTexImage3D: 0x{:X}", err);
        }
    } else if (texture.target == GL_TEXTURE_CUBE_MAP) {

        for (int face = 0; face < 6; ++face) {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, internalFormat,
                        desc.width, desc.height, 0, format, dataType, nullptr);
        }
        err = glGetError();
        if (err != GL_NO_ERROR) {
            LOG_ERROR(LogCategory::Render, "OpenGL error after glTexImage2D (cubemap): 0x{:X}", err);
        }
    } else if (texture.target == GL_TEXTURE_2D_ARRAY) {
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, internalFormat, desc.width, desc.height, desc.arrayLayers,
                     0, format, dataType, desc.initialData);
        err = glGetError();
        if (err != GL_NO_ERROR) {
            LOG_ERROR(LogCategory::Render, "OpenGL error after glTexImage3D (2D array): 0x{:X}", err);
        }
    } else if (texture.target == GL_TEXTURE_CUBE_MAP_ARRAY) {

        glTexImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, 0, internalFormat, desc.width, desc.height, desc.arrayLayers * 6,
                     0, format, dataType, desc.initialData);
        err = glGetError();
        if (err != GL_NO_ERROR) {
            LOG_ERROR(LogCategory::Render, "OpenGL error after glTexImage3D (cubemap array): 0x{:X}", err);
        }
    }

    if (desc.mipLevels > 1) {
        glGenerateMipmap(texture.target);
    }

    GLenum minFilter = (m_impl->defaultMinFilter == FilterMode::Nearest) ? GL_NEAREST : GL_LINEAR;
    GLenum magFilter = (m_impl->defaultMagFilter == FilterMode::Nearest) ? GL_NEAREST : GL_LINEAR;

    if (desc.mipLevels > 1) {
        minFilter = (m_impl->defaultMinFilter == FilterMode::Nearest) ? GL_NEAREST_MIPMAP_NEAREST : GL_LINEAR_MIPMAP_LINEAR;
    }

    glTexParameteri(texture.target, GL_TEXTURE_MIN_FILTER, minFilter);
    glTexParameteri(texture.target, GL_TEXTURE_MAG_FILTER, magFilter);
    glTexParameteri(texture.target, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(texture.target, GL_TEXTURE_WRAP_T, GL_REPEAT);
    if (texture.target == GL_TEXTURE_3D || texture.target == GL_TEXTURE_CUBE_MAP) {
        glTexParameteri(texture.target, GL_TEXTURE_WRAP_R, GL_REPEAT);
    }

    glBindTexture(texture.target, 0);

    texture.ownerContext = m_impl->state.currentContext;

    uint32_t id = m_impl->state.nextTextureID++;
    m_impl->state.textures[id] = texture;

    return TextureHandle(id);
}

void GfxDeviceOpenGL::destroyTexture(TextureHandle texture) {
    auto it = m_impl->state.textures.find(texture.id);
    if (it != m_impl->state.textures.end()) {
        GLuint glTextureId = it->second.id;
        glDeleteTextures(1, &glTextureId);

        for (size_t i = 0; i < m_impl->state.boundTextures.size(); ++i) {
            if (m_impl->state.boundTextures[i].id == glTextureId) {
                m_impl->state.boundTextures[i].id = 0;
                m_impl->state.boundTextures[i].target = 0;
            }
        }

        m_impl->state.textures.erase(it);
    }
}

void GfxDeviceOpenGL::updateTexture(TextureHandle texture, const void* data,
                                     uint32_t mipLevel, uint32_t arrayLayer) {
    auto it = m_impl->state.textures.find(texture.id);
    if (it == m_impl->state.textures.end() || !data) {
        return;
    }

    const GLTexture& glTexture = it->second;
    glBindTexture(glTexture.target, glTexture.id);

    GLenum format = GLUtils::toGLTextureFormat(glTexture.format);
    GLenum dataType = GLUtils::toGLDataType(glTexture.format);

    if (glTexture.target == GL_TEXTURE_2D) {
        glTexSubImage2D(GL_TEXTURE_2D, mipLevel, 0, 0,
                        glTexture.width, glTexture.height,
                        format, dataType, data);
    }

    glBindTexture(glTexture.target, 0);
}

BufferHandle GfxDeviceOpenGL::createBuffer(const BufferDesc& desc) {
    GLBuffer buffer;
    buffer.size = desc.size;
    buffer.usage = desc.usage;

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

    if (m_impl->state.allContexts.size() == 1) {
        buffer.ownerContext = m_impl->state.currentContext;
    } else {

        bool isIsolated = true;
        void* currentCtx = m_impl->state.currentContext;
        for (const auto& [id, swapchain] : m_impl->state.swapchains) {
            if (swapchain.contextHandle == currentCtx) {

                break;
            }
        }
        buffer.ownerContext = m_impl->state.currentContext;
    }

    uint32_t id = m_impl->state.nextBufferID++;
    m_impl->state.buffers[id] = buffer;

    return BufferHandle(id);
}

void GfxDeviceOpenGL::destroyBuffer(BufferHandle buffer) {
    auto it = m_impl->state.buffers.find(buffer.id);
    if (it != m_impl->state.buffers.end()) {
        glDeleteBuffers(1, &it->second.id);
        m_impl->state.buffers.erase(it);
    }
}

void GfxDeviceOpenGL::updateBuffer(BufferHandle buffer, const void* data,
                                     uint64_t size, uint64_t offset) {
    auto it = m_impl->state.buffers.find(buffer.id);
    if (it == m_impl->state.buffers.end() || !data) {
        return;
    }

    const GLBuffer& glBuffer = it->second;

    if (glBuffer.target == GL_ELEMENT_ARRAY_BUFFER) {

        GLint previousVAO = 0;
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVAO);

        if (previousVAO != 0) {
            glBindVertexArray(0);
        }

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, glBuffer.id);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, offset, size, data);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        if (previousVAO != 0) {
            glBindVertexArray(previousVAO);
        }

        m_impl->state.boundVAO = previousVAO;

    } else {

        GLint previousBinding = 0;
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previousBinding);

        glBindBuffer(GL_ARRAY_BUFFER, glBuffer.id);
        glBufferSubData(GL_ARRAY_BUFFER, offset, size, data);
        glBindBuffer(GL_ARRAY_BUFFER, previousBinding);

        m_impl->state.boundArrayBuffer = previousBinding;
    }
}

SamplerHandle GfxDeviceOpenGL::createSampler(const SamplerDesc& desc) {
    GLSampler sampler;
    sampler.desc = desc;

    glGenSamplers(1, &sampler.id);

    glSamplerParameteri(sampler.id, GL_TEXTURE_MIN_FILTER, GLUtils::toGLFilterMode(desc.minFilter));
    glSamplerParameteri(sampler.id, GL_TEXTURE_MAG_FILTER, GLUtils::toGLFilterMode(desc.magFilter));

    glSamplerParameteri(sampler.id, GL_TEXTURE_WRAP_S, GLUtils::toGLWrapMode(desc.wrapU));
    glSamplerParameteri(sampler.id, GL_TEXTURE_WRAP_R, GLUtils::toGLWrapMode(desc.wrapV));
    glSamplerParameteri(sampler.id, GL_TEXTURE_WRAP_T, GLUtils::toGLWrapMode(desc.wrapW));

    if (desc.maxAnisotropy > 1.0f && GLEW_EXT_texture_filter_anisotropic) {
        glSamplerParameterf(sampler.id, GL_TEXTURE_MAX_ANISOTROPY_EXT, desc.maxAnisotropy);
    }

    if (desc.compareEnable) {
        glSamplerParameteri(sampler.id, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glSamplerParameteri(sampler.id, GL_TEXTURE_COMPARE_FUNC, GLUtils::toGLCompareFunc(desc.compareFunc));
    }

    uint32_t id = m_impl->state.nextSamplerID++;
    m_impl->state.samplers[id] = sampler;

    return SamplerHandle(id);
}

void GfxDeviceOpenGL::destroySampler(SamplerHandle sampler) {
    auto it = m_impl->state.samplers.find(sampler.id);
    if (it != m_impl->state.samplers.end()) {
        glDeleteSamplers(1, &it->second.id);
        m_impl->state.samplers.erase(it);
    }
}

ShaderHandle GfxDeviceOpenGL::createShader(const ShaderDesc& desc) {
    GLShader shader;
    shader.stage = desc.stage;

    GLenum shaderType = GLUtils::toGLShaderStage(desc.stage);
    shader.id = glCreateShader(shaderType);

    const char* source = static_cast<const char*>(desc.bytecode);
    GLint length = static_cast<GLint>(desc.bytecodeSize);

    glShaderSource(shader.id, 1, &source, &length);
    glCompileShader(shader.id);

    GLint success;
    glGetShaderiv(shader.id, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader.id, 512, nullptr, infoLog);

        glDeleteShader(shader.id);
        return ShaderHandle();
    }

    uint32_t id = m_impl->state.nextShaderID++;
    m_impl->state.shaders[id] = shader;

    return ShaderHandle(id);
}

void GfxDeviceOpenGL::destroyShader(ShaderHandle shader) {
    auto it = m_impl->state.shaders.find(shader.id);
    if (it != m_impl->state.shaders.end()) {
        glDeleteShader(it->second.id);
        m_impl->state.shaders.erase(it);
    }
}

PipelineHandle GfxDeviceOpenGL::createPipeline(const PipelineDesc& desc) {
    GLPipeline pipeline;
    pipeline.shaders = desc.shaders;
    pipeline.vertexLayout = desc.vertexLayout;
    pipeline.topology = desc.topology;
    pipeline.blendState = desc.blendState;
    pipeline.depthStencilState = desc.depthStencilState;
    pipeline.rasterizerState = desc.rasterizerState;

    pipeline.program = glCreateProgram();

    for (const ShaderHandle& shaderHandle : desc.shaders) {
        auto it = m_impl->state.shaders.find(shaderHandle.id);
        if (it != m_impl->state.shaders.end()) {
            glAttachShader(pipeline.program, it->second.id);
        }
    }

    glLinkProgram(pipeline.program);

    GLint success;
    glGetProgramiv(pipeline.program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(pipeline.program, 512, nullptr, infoLog);

        glDeleteProgram(pipeline.program);
        return PipelineHandle();
    }

    for (const ShaderHandle& shaderHandle : desc.shaders) {
        auto it = m_impl->state.shaders.find(shaderHandle.id);
        if (it != m_impl->state.shaders.end()) {
            glDetachShader(pipeline.program, it->second.id);
        }
    }

    glUseProgram(pipeline.program);

    GLuint lightDataBlockIndex = glGetUniformBlockIndex(pipeline.program, "LightData");
    if (lightDataBlockIndex != GL_INVALID_INDEX) {
        glUniformBlockBinding(pipeline.program, lightDataBlockIndex, 0);

    }

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

void GfxDeviceOpenGL::destroyPipeline(PipelineHandle pipeline) {
    auto it = m_impl->state.pipelines.find(pipeline.id);
    if (it != m_impl->state.pipelines.end()) {
        glDeleteProgram(it->second.program);

        for (const auto& [context, vao] : it->second.vaos) {
            glDeleteVertexArrays(1, &vao);
        }

        m_impl->state.pipelines.erase(it);
    }
}

RenderTargetHandle GfxDeviceOpenGL::createRenderTarget(const RenderTargetDesc& desc) {
    GLRenderTarget renderTarget;
    renderTarget.width = desc.width;
    renderTarget.height = desc.height;
    renderTarget.colorFormat = desc.colorFormat;
    renderTarget.depthFormat = desc.depthFormat;
    renderTarget.isCubeMap = desc.isCubeMap;
    renderTarget.currentCubeFace = desc.cubeMapFace;

    glGenFramebuffers(1, &renderTarget.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, renderTarget.fbo);

    if (desc.hasColor) {

        GLenum colorTarget = desc.isCubeMap ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D;
        if (desc.sampleCount > 1 && !desc.isCubeMap) {
            colorTarget = GL_TEXTURE_2D_MULTISAMPLE;
        }

        glGenTextures(1, &renderTarget.colorTexture);
        glBindTexture(colorTarget, renderTarget.colorTexture);

        GLenum colorInternalFormat = GLUtils::toGLInternalFormat(desc.colorFormat);
        GLenum colorFormat = GLUtils::toGLTextureFormat(desc.colorFormat);
        GLenum colorType = GLUtils::toGLDataType(desc.colorFormat);

        if (desc.isCubeMap) {

            for (int face = 0; face < 6; ++face) {
                glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, colorInternalFormat,
                            desc.width, desc.height, 0, colorFormat, colorType, nullptr);
            }
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

            if (desc.cubeMapFace >= 0 && desc.cubeMapFace < 6) {
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                      GL_TEXTURE_CUBE_MAP_POSITIVE_X + desc.cubeMapFace,
                                      renderTarget.colorTexture, 0);
            }
        } else if (desc.sampleCount > 1) {

            glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, desc.sampleCount,
                                   colorInternalFormat, desc.width, desc.height, GL_TRUE);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                  GL_TEXTURE_2D_MULTISAMPLE, renderTarget.colorTexture, 0);
        } else {
            glTexImage2D(GL_TEXTURE_2D, 0, colorInternalFormat, desc.width, desc.height,
                        0, colorFormat, colorType, nullptr);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                  GL_TEXTURE_2D, renderTarget.colorTexture, 0);
        }

        GLTexture colorTex;
        colorTex.id = renderTarget.colorTexture;
        colorTex.width = desc.width;
        colorTex.height = desc.height;
        colorTex.format = desc.colorFormat;
        colorTex.target = colorTarget;
        colorTex.ownerContext = nullptr;
        uint32_t colorTexID = m_impl->state.nextTextureID++;
        m_impl->state.textures[colorTexID] = colorTex;
        renderTarget.colorTextureHandle = TextureHandle(colorTexID);
    } else {

        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
    }

    GLenum depthTarget = desc.isCubeMap ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D;
    if (desc.sampleCount > 1 && !desc.isCubeMap) {
        depthTarget = GL_TEXTURE_2D_MULTISAMPLE;
    }

    if (desc.hasDepth) {
        glGenTextures(1, &renderTarget.depthStencilTexture);
        glBindTexture(depthTarget, renderTarget.depthStencilTexture);

        GLenum depthInternalFormat = GLUtils::toGLInternalFormat(desc.depthFormat);
        GLenum depthFormat = GLUtils::toGLTextureFormat(desc.depthFormat);
        GLenum depthType = GLUtils::toGLDataType(desc.depthFormat);

        if (desc.isCubeMap) {

            for (int face = 0; face < 6; ++face) {
                glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, depthInternalFormat,
                            desc.width, desc.height, 0, depthFormat, depthType, nullptr);

                GLenum error = glGetError();
                if (error != GL_NO_ERROR) {

                }
            }

            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_MODE, GL_NONE);

            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BASE_LEVEL, 0);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, 0);

            bool cubeMapComplete = true;
            for (int face = 0; face < 6; ++face) {
                GLint width = 0, height = 0, format = 0;
                glGetTexLevelParameteriv(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_TEXTURE_WIDTH, &width);
                glGetTexLevelParameteriv(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_TEXTURE_HEIGHT, &height);
                glGetTexLevelParameteriv(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_TEXTURE_INTERNAL_FORMAT, &format);

                if (width != static_cast<GLint>(desc.width) || height != static_cast<GLint>(desc.height)) {

                    cubeMapComplete = false;
                }
            }

            if (cubeMapComplete) {

            } else {

            }

            GLenum error = glGetError();
            if (error != GL_NO_ERROR) {

            }
        } else if (desc.sampleCount > 1) {
            glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, desc.sampleCount,
                                   depthInternalFormat, desc.width, desc.height, GL_TRUE);
        } else {
            glTexImage2D(GL_TEXTURE_2D, 0, depthInternalFormat, desc.width, desc.height,
                        0, depthFormat, depthType, nullptr);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
        }

        GLenum attachment = (desc.depthFormat == TextureFormat::DEPTH24_STENCIL8 ||
                            desc.depthFormat == TextureFormat::DEPTH32F_STENCIL8) ?
                            GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT;

        if (desc.isCubeMap && desc.cubeMapFace >= 0 && desc.cubeMapFace < 6) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, attachment,
                                  GL_TEXTURE_CUBE_MAP_POSITIVE_X + desc.cubeMapFace,
                                  renderTarget.depthStencilTexture, 0);
        } else if (!desc.isCubeMap) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, attachment,
                                  depthTarget, renderTarget.depthStencilTexture, 0);
        }

        GLTexture depthTex;
        depthTex.id = renderTarget.depthStencilTexture;
        depthTex.width = desc.width;
        depthTex.height = desc.height;
        depthTex.format = desc.depthFormat;
        depthTex.target = depthTarget;
        depthTex.ownerContext = nullptr;
        uint32_t depthTexID = m_impl->state.nextTextureID++;
        m_impl->state.textures[depthTexID] = depthTex;
        renderTarget.depthTextureHandle = TextureHandle(depthTexID);
    }

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {

        glDeleteFramebuffers(1, &renderTarget.fbo);
        glDeleteTextures(1, &renderTarget.colorTexture);
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

void GfxDeviceOpenGL::destroyRenderTarget(RenderTargetHandle target) {
    auto it = m_impl->state.renderTargets.find(target.id);
    if (it != m_impl->state.renderTargets.end()) {
        const GLRenderTarget& rt = it->second;

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
        }

        m_impl->state.renderTargets.erase(it);
    }
}

TextureHandle GfxDeviceOpenGL::getRenderTargetColorTexture(RenderTargetHandle target) {
    auto it = m_impl->state.renderTargets.find(target.id);
    if (it != m_impl->state.renderTargets.end()) {
        return it->second.colorTextureHandle;
    }
    return TextureHandle();
}

TextureHandle GfxDeviceOpenGL::getRenderTargetDepthTexture(RenderTargetHandle target) {
    auto it = m_impl->state.renderTargets.find(target.id);
    if (it != m_impl->state.renderTargets.end()) {
        return it->second.depthTextureHandle;
    }
    return TextureHandle();
}

void GfxDeviceOpenGL::attachCubeMapFace(RenderTargetHandle target, uint32_t faceIndex) {
    if (faceIndex > 5) {

        return;
    }

    auto it = m_impl->state.renderTargets.find(target.id);
    if (it == m_impl->state.renderTargets.end()) {

        return;
    }

    GLRenderTarget& rt = it->second;
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

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {

    }

    rt.currentCubeFace = faceIndex;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GfxDeviceOpenGL::unbindFramebuffer() {

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    m_impl->state.boundFramebuffer = 0;
}

UniformBufferHandle GfxDeviceOpenGL::createUniformBuffer(uint32_t size) {
    GLUniformBuffer ubo;
    ubo.size = size;

    glGenBuffers(1, &ubo.id);
    glBindBuffer(GL_UNIFORM_BUFFER, ubo.id);
    glBufferData(GL_UNIFORM_BUFFER, size, nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    uint32_t id = m_impl->state.nextUniformBufferID++;
    m_impl->state.uniformBuffers[id] = ubo;

    return UniformBufferHandle(id);
}

void GfxDeviceOpenGL::destroyUniformBuffer(UniformBufferHandle buffer) {
    auto it = m_impl->state.uniformBuffers.find(buffer.id);
    if (it != m_impl->state.uniformBuffers.end()) {
        glDeleteBuffers(1, &it->second.id);
        m_impl->state.uniformBuffers.erase(it);
    }
}

void GfxDeviceOpenGL::updateUniformBuffer(UniformBufferHandle buffer, const void* data,
                                           uint32_t size, uint32_t offset) {
    auto it = m_impl->state.uniformBuffers.find(buffer.id);
    if (it == m_impl->state.uniformBuffers.end() || !data) {
        return;
    }

    const GLUniformBuffer& ubo = it->second;
    glBindBuffer(GL_UNIFORM_BUFFER, ubo.id);
    glBufferSubData(GL_UNIFORM_BUFFER, offset, size, data);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

std::unique_ptr<IGfxCommandList> GfxDeviceOpenGL::beginFrame(RenderTargetHandle target) {

#ifdef _WIN32
    bool foundSwapchain = false;
    for (const auto& [id, swapchain] : m_impl->state.swapchains) {
        if (swapchain.backbuffer.id == target.id && swapchain.contextHandle && swapchain.deviceContext) {
            foundSwapchain = true;
            HDC hdc = static_cast<HDC>(swapchain.deviceContext);
            HGLRC hglrc = static_cast<HGLRC>(swapchain.contextHandle);

            bool contextChanged = (m_impl->state.currentContext != hglrc);

            if (!wglMakeCurrent(hdc, hglrc)) {

            } else {

                if (contextChanged) {

                    m_impl->state.currentContext = hglrc;
                    m_impl->state.boundProgram = UINT32_MAX;
                    m_impl->state.boundVAO = UINT32_MAX;
                    m_impl->state.boundArrayBuffer = UINT32_MAX;
                    m_impl->state.boundElementBuffer = UINT32_MAX;
                    m_impl->state.boundFramebuffer = UINT32_MAX;

                    for (auto& binding : m_impl->state.boundTextures) {
                        binding.id = UINT32_MAX;
                        binding.target = 0;
                    }
                    for (auto& sampler : m_impl->state.boundSamplers) {
                        sampler = 0;
                    }

                    m_impl->state.viewport = {0, 0, 0, 0, 0, 1};
                    m_impl->state.scissor = {0, 0, 0, 0, false};

                    glEnable(GL_DEPTH_TEST);
                    glDepthFunc(GL_LESS);
                    glDepthMask(GL_TRUE);
                }

                glDisable(GL_SCISSOR_TEST);

                glScissor(0, 0, swapchain.width, swapchain.height);

                m_impl->state.scissor = {0, 0, swapchain.width, swapchain.height, false};
            }
            break;
        }
    }

    if (!foundSwapchain && !m_impl->state.swapchains.empty()) {
        const auto& firstSwapchain = m_impl->state.swapchains.begin()->second;
        if (firstSwapchain.contextHandle && firstSwapchain.deviceContext) {
            HDC hdc = static_cast<HDC>(firstSwapchain.deviceContext);
            HGLRC hglrc = static_cast<HGLRC>(firstSwapchain.contextHandle);

            bool contextChanged = (m_impl->state.currentContext != hglrc);

            if (!wglMakeCurrent(hdc, hglrc)) {

            } else {
                if (contextChanged) {

                    m_impl->state.currentContext = hglrc;
                    m_impl->state.boundProgram = UINT32_MAX;
                    m_impl->state.boundVAO = UINT32_MAX;
                    m_impl->state.boundArrayBuffer = UINT32_MAX;
                    m_impl->state.boundElementBuffer = UINT32_MAX;
                    m_impl->state.boundFramebuffer = UINT32_MAX;

                    for (auto& binding : m_impl->state.boundTextures) {
                        binding.id = UINT32_MAX;
                        binding.target = 0;
                    }
                    for (auto& sampler : m_impl->state.boundSamplers) {
                        sampler = 0;
                    }

                    m_impl->state.viewport = {0, 0, 0, 0, 0, 1};
                    m_impl->state.scissor = {0, 0, 0, 0, false};
                }

                glDisable(GL_SCISSOR_TEST);

                auto rtIt = m_impl->state.renderTargets.find(target.id);
                if (rtIt != m_impl->state.renderTargets.end()) {
                    const GLRenderTarget& rt = rtIt->second;
                    glScissor(0, 0, rt.width, rt.height);
                    m_impl->state.scissor = {0, 0, rt.width, rt.height, false};
                } else {

                    glScissor(0, 0, 0, 0);
                    m_impl->state.scissor = {0, 0, 0, 0, false};
                }
            }
        }
    }
#endif

    auto cmd = std::make_unique<GfxCommandListOpenGL>(&m_impl->state);

    cmd->setRenderTarget(target);

    return cmd;
}

void GfxDeviceOpenGL::submit(std::unique_ptr<IGfxCommandList> commandList) {
    if (auto* glCmd = dynamic_cast<GfxCommandListOpenGL*>(commandList.get())) {
        glCmd->execute();
    }

}

void GfxDeviceOpenGL::waitIdle() {
    glFinish();
}

MeshHandle GfxDeviceOpenGL::createMesh(const MeshData& meshData) {
    GPUMesh gpuMesh;

    BufferDesc vbDesc;
    vbDesc.size = meshData.vertices.size() * sizeof(Vertex);
    vbDesc.usage = BufferUsage::Vertex;
    vbDesc.initialData = meshData.vertices.data();

    gpuMesh.vertexBuffer = createBuffer(vbDesc);
    gpuMesh.vertexCount = static_cast<uint32_t>(meshData.vertices.size());

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

const GPUMesh* GfxDeviceOpenGL::getMesh(MeshHandle handle) const {
    auto it = m_impl->meshes.find(handle.id);
    if (it != m_impl->meshes.end()) {
        return &it->second;
    }
    return nullptr;
}

void GfxDeviceOpenGL::destroyMesh(MeshHandle handle) {
    auto it = m_impl->meshes.find(handle.id);
    if (it != m_impl->meshes.end()) {
        destroyBuffer(it->second.vertexBuffer);
        destroyBuffer(it->second.indexBuffer);
        m_impl->meshes.erase(it);
    }
}

#include <stb_truetype.h>

FontHandle GfxDeviceOpenGL::createFontAtlas(const FontDesc& desc) {

    FILE* fontFile = fopen(desc.fontPath.c_str(), "rb");
    if (!fontFile) {

        return FontHandle();
    }

    fseek(fontFile, 0, SEEK_END);
    long fileSize = ftell(fontFile);
    fseek(fontFile, 0, SEEK_SET);

    std::vector<unsigned char> fontBuffer(fileSize);
    fread(fontBuffer.data(), 1, fileSize, fontFile);
    fclose(fontFile);

    stbtt_fontinfo fontInfo;
    if (!stbtt_InitFont(&fontInfo, fontBuffer.data(), 0)) {

        return FontHandle();
    }

    std::vector<unsigned char> atlasBitmap(desc.atlasWidth * desc.atlasHeight, 0);

    stbtt_bakedchar bakedChars[256];
    float pixelHeight = desc.fontSize;

    if (desc.charRanges.empty()) {

        return FontHandle();
    }

    const auto& charRange = desc.charRanges[0];
    uint32_t numChars = charRange.last - charRange.first + 1;

    int result = stbtt_BakeFontBitmap(
        fontBuffer.data(), 0,
        pixelHeight,
        atlasBitmap.data(), desc.atlasWidth, desc.atlasHeight,
        charRange.first, numChars,
        bakedChars
    );

    if (result <= 0) {

        return FontHandle();
    }

    TextureDesc texDesc;
    texDesc.width = desc.atlasWidth;
    texDesc.height = desc.atlasHeight;
    texDesc.format = TextureFormat::R8_UNORM;
    texDesc.mipLevels = 1;
    texDesc.usage = TextureUsage::Sampled;
    texDesc.initialData = atlasBitmap.data();

    TextureHandle atlasTexture = createTexture(texDesc);

    FontAtlas fontAtlas;
    fontAtlas.texture = atlasTexture;
    fontAtlas.atlasWidth = desc.atlasWidth;
    fontAtlas.atlasHeight = desc.atlasHeight;
    fontAtlas.fontSize = desc.fontSize;

    int ascent, descent, lineGap;
    float scale = stbtt_ScaleForPixelHeight(&fontInfo, pixelHeight);
    stbtt_GetFontVMetrics(&fontInfo, &ascent, &descent, &lineGap);
    fontAtlas.lineHeight = (ascent - descent + lineGap) * scale;

    for (uint32_t i = 0; i < numChars; ++i) {
        const stbtt_bakedchar& bc = bakedChars[i];

        Glyph glyph;
        glyph.codepoint = charRange.first + i;
        glyph.uvMin = Vec2(bc.x0 / (float)desc.atlasWidth, bc.y0 / (float)desc.atlasHeight);
        glyph.uvMax = Vec2(bc.x1 / (float)desc.atlasWidth, bc.y1 / (float)desc.atlasHeight);
        glyph.size = Vec2(bc.x1 - bc.x0, bc.y1 - bc.y0);
        glyph.bearing = Vec2(bc.xoff, bc.yoff);
        glyph.advance = bc.xadvance;

        fontAtlas.glyphs[glyph.codepoint] = glyph;
    }

    uint32_t fontID = m_impl->nextFontID++;
    m_impl->fonts[fontID] = fontAtlas;

    return FontHandle(fontID);
}

const FontAtlas* GfxDeviceOpenGL::getFontAtlas(FontHandle handle) const {
    if (!handle.isValid()) {
        return nullptr;
    }

    auto it = m_impl->fonts.find(handle.id);
    if (it != m_impl->fonts.end()) {
        return &it->second;
    }
    return nullptr;
}

void GfxDeviceOpenGL::destroyFontAtlas(FontHandle handle) {
    if (!handle.isValid()) {
        return;
    }

    auto it = m_impl->fonts.find(handle.id);
    if (it != m_impl->fonts.end()) {
        destroyTexture(it->second.texture);
        m_impl->fonts.erase(it);

    }
}

}
