#include "lupine/rendering/backends/opengl/GfxCommandListOpenGL.hpp"
#include "lupine/rendering/backends/opengl/OpenGLState.hpp"
#include "lupine/logger/Logger.hpp"
#include <GL/glew.h>
#include <glm/gtc/type_ptr.hpp>

namespace lupine {

GfxCommandListOpenGL::GfxCommandListOpenGL(OpenGLState* state)
    : m_state(state) {
    m_vertexBuffers.resize(8);
}

GfxCommandListOpenGL::~GfxCommandListOpenGL() {
}

void GfxCommandListOpenGL::setRenderTarget(RenderTargetHandle target) {
    m_currentRenderTarget = target;

    auto it = m_state->renderTargets.find(target.id);
    if (it != m_state->renderTargets.end()) {
        const GLRenderTarget& rt = it->second;
        m_state->bindFramebuffer(rt.fbo);

        if (rt.fbo == 0) {

            glDrawBuffer(GL_BACK);
            glReadBuffer(GL_BACK);
        } else {

            if (rt.colorTexture == 0) {

                glDrawBuffer(GL_NONE);
                glReadBuffer(GL_NONE);
            } else {

                glDrawBuffer(GL_COLOR_ATTACHMENT0);
                glReadBuffer(GL_COLOR_ATTACHMENT0);
            }
        }
    } else {

    }
}

void GfxCommandListOpenGL::setViewport(const Viewport& viewport) {
    glViewport(
        static_cast<GLint>(viewport.x),
        static_cast<GLint>(viewport.y),
        static_cast<GLsizei>(viewport.width),
        static_cast<GLsizei>(viewport.height)
    );

    glDepthRangef(viewport.minDepth, viewport.maxDepth);

    m_state->viewport.x = viewport.x;
    m_state->viewport.y = viewport.y;
    m_state->viewport.width = viewport.width;
    m_state->viewport.height = viewport.height;
    m_state->viewport.minDepth = viewport.minDepth;
    m_state->viewport.maxDepth = viewport.maxDepth;
}

void GfxCommandListOpenGL::setScissor(const ScissorRect& scissor) {
    if (scissor.width > 0 && scissor.height > 0) {
        glEnable(GL_SCISSOR_TEST);
        // The ScissorRect contract is top-left origin framebuffer pixels (matching
        // the Viewport and the DX/Vulkan backends). glScissor uses a bottom-left
        // origin, so flip Y using the active viewport (which covers the full
        // render target in this engine).
        GLint flippedY = static_cast<GLint>(m_state->viewport.y + m_state->viewport.height)
                       - (static_cast<GLint>(scissor.y) + static_cast<GLint>(scissor.height));
        glScissor(scissor.x, flippedY, scissor.width, scissor.height);
        m_state->scissor.enabled = true;

    } else {
        glDisable(GL_SCISSOR_TEST);
        m_state->scissor.enabled = false;

    }

    m_state->scissor.x = scissor.x;
    m_state->scissor.y = scissor.y;
    m_state->scissor.width = scissor.width;
    m_state->scissor.height = scissor.height;
}

void GfxCommandListOpenGL::clearColor(const Color& color) {
    glClearColor(color.r, color.g, color.b, color.a);

    m_pendingColorClear = true;
}

void GfxCommandListOpenGL::clearDepth(float depth) {

    glDepthMask(GL_TRUE);

    glClearDepth(depth);

    GLbitfield clearFlags = GL_DEPTH_BUFFER_BIT;
    if (m_pendingColorClear) {
        clearFlags |= GL_COLOR_BUFFER_BIT;
        m_pendingColorClear = false;
    }

    glClear(clearFlags);

    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        LOG_ERROR(LogCategory::Render, "[GL] glClear failed: 0x{:04X}", error);
    }
}

void GfxCommandListOpenGL::clearStencil(uint8_t stencil) {
    glClearStencil(stencil);
    glClear(GL_STENCIL_BUFFER_BIT);
}

void GfxCommandListOpenGL::bindPipeline(PipelineHandle pipeline) {
    m_currentPipeline = pipeline;

    auto it = m_state->pipelines.find(pipeline.id);
    if (it == m_state->pipelines.end()) {
        return;
    }

    GLPipeline& glPipeline = const_cast<GLPipeline&>(it->second);

    m_state->bindProgram(glPipeline.program);

    void* currentContext = m_state->currentContext;
    GLuint vao = 0;

    auto vaoIt = glPipeline.vaos.find(currentContext);
    if (vaoIt == glPipeline.vaos.end()) {

        glGenVertexArrays(1, &vao);

        GLenum error = glGetError();
        if (error != GL_NO_ERROR) {

        }

        glPipeline.vaos[currentContext] = vao;
        glPipeline.vaoInitialized[currentContext] = false;

    } else {
        vao = vaoIt->second;
    }

    m_state->bindVAO(vao);

    if (glPipeline.blendState.blendEnable) {
        glEnable(GL_BLEND);
        glBlendFuncSeparate(
            GLUtils::toGLBlendFactor(glPipeline.blendState.srcColorBlend),
            GLUtils::toGLBlendFactor(glPipeline.blendState.dstColorBlend),
            GLUtils::toGLBlendFactor(glPipeline.blendState.srcAlphaBlend),
            GLUtils::toGLBlendFactor(glPipeline.blendState.dstAlphaBlend)
        );
        glBlendEquationSeparate(
            GLUtils::toGLBlendOp(glPipeline.blendState.colorBlendOp),
            GLUtils::toGLBlendOp(glPipeline.blendState.alphaBlendOp)
        );
    } else {
        glDisable(GL_BLEND);
    }

    if (glPipeline.depthStencilState.depthTestEnable) {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GLUtils::toGLCompareFunc(glPipeline.depthStencilState.depthCompareFunc));
    } else {
        glDisable(GL_DEPTH_TEST);
    }

    glDepthMask(glPipeline.depthStencilState.depthWriteEnable ? GL_TRUE : GL_FALSE);

    if (glPipeline.depthStencilState.stencilEnable) {
        glEnable(GL_STENCIL_TEST);

        glStencilFunc(GL_ALWAYS, 0, 0xFF);

        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

        glStencilMask(0xFF);
    } else {
        glDisable(GL_STENCIL_TEST);
    }

    switch (glPipeline.rasterizerState.cullMode) {
        case CullMode::None:
            glDisable(GL_CULL_FACE);
            break;
        case CullMode::Front:
            glEnable(GL_CULL_FACE);
            glCullFace(GL_FRONT);
            break;
        case CullMode::Back:
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);
            break;
    }

    glFrontFace(glPipeline.rasterizerState.frontFace == WindingOrder::Clockwise ? GL_CW : GL_CCW);

    glPolygonMode(GL_FRONT_AND_BACK,
        glPipeline.rasterizerState.fillMode == FillMode::Wireframe ? GL_LINE : GL_FILL);

}

void GfxCommandListOpenGL::setLineWidth(float width) {
    glLineWidth(width);
}

void GfxCommandListOpenGL::bindVertexBuffer(BufferHandle buffer, uint32_t binding, uint64_t offset) {

    if (binding < m_vertexBuffers.size()) {
        m_vertexBuffers[binding].buffer = buffer;
        m_vertexBuffers[binding].offset = offset;

        auto bufferIt = m_state->buffers.find(buffer.id);
        if (bufferIt != m_state->buffers.end()) {
            const GLBuffer& glBuffer = bufferIt->second;

            if (glBuffer.id == 0) {

                return;
            }

            m_state->bindBuffer(GL_ARRAY_BUFFER, glBuffer.id);

            auto pipelineIt = m_state->pipelines.find(m_currentPipeline.id);
            if (pipelineIt != m_state->pipelines.end()) {
                GLPipeline& pipeline = const_cast<GLPipeline&>(pipelineIt->second);

                while (glGetError() != GL_NO_ERROR);

                // Select the layout that owns this binding: binding 0 is the
                // primary geometry layout; higher bindings come from the
                // per-instance / extra vertex buffers. Attributes are set up
                // against the currently bound GL_ARRAY_BUFFER and tagged with the
                // appropriate attribute divisor (1 for per-instance data).
                const VertexBufferLayout* layout = nullptr;
                if (binding == 0) {
                    layout = &pipeline.vertexLayout;
                } else {
                    for (const auto& extra : pipeline.extraVertexBuffers) {
                        if (!extra.attributes.empty() && extra.attributes.front().binding == binding) {
                            layout = &extra;
                            break;
                        }
                    }
                }

                if (layout) {
                    const GLuint divisor = (layout->inputRate == VertexInputRate::Instance) ? 1u : 0u;
                    for (const auto& attr : layout->attributes) {
                        if (attr.binding != binding) {
                            continue;
                        }
                        GLint location = glGetAttribLocation(pipeline.program, attr.name.c_str());
                        if (location >= 0) {
                            GLenum type = GLUtils::getVertexFormatType(attr.format);
                            GLint count = GLUtils::getVertexFormatCount(attr.format);
                            GLboolean normalized = GLUtils::isVertexFormatNormalized(attr.format) ? GL_TRUE : GL_FALSE;

                            glEnableVertexAttribArray(location);

                            glVertexAttribPointer(
                                location,
                                count,
                                type,
                                normalized,
                                layout->stride,
                                reinterpret_cast<void*>(static_cast<uintptr_t>(attr.offset))
                            );

                            // Per-instance attributes advance once per instance.
                            glVertexAttribDivisor(location, divisor);

                            GLenum error = glGetError();
                            if (error != GL_NO_ERROR) {

                            }
                        }
                    }
                }
            }
        }
    }
}

void GfxCommandListOpenGL::bindIndexBuffer(BufferHandle buffer, IndexFormat format, uint64_t offset) {

    m_currentIndexBuffer = buffer;
    m_currentIndexFormat = format;
    m_indexBufferOffset = offset;

    auto it = m_state->buffers.find(buffer.id);
    if (it != m_state->buffers.end()) {
        const GLBuffer& glBuffer = it->second;

        if (glBuffer.id == 0) {

            return;
        }

        m_state->bindBuffer(GL_ELEMENT_ARRAY_BUFFER, glBuffer.id);
    } else {

    }
}

void GfxCommandListOpenGL::bindUniformBuffer(UniformBufferHandle buffer, uint32_t binding, uint32_t) {
    auto it = m_state->uniformBuffers.find(buffer.id);
    if (it != m_state->uniformBuffers.end()) {
        const GLUniformBuffer& glBuffer = it->second;
        glBindBufferBase(GL_UNIFORM_BUFFER, binding, glBuffer.id);
    }
}

void GfxCommandListOpenGL::bindTexture(TextureHandle texture, uint32_t binding, uint32_t) {
    auto it = m_state->textures.find(texture.id);
    if (it != m_state->textures.end()) {
        const GLTexture& glTexture = it->second;

        m_state->bindTexture(binding, glTexture.target, glTexture.id);
    } else {
        // Unbind texture slot when handle is invalid to prevent stale texture bleeding
        m_state->bindTexture(binding, GL_TEXTURE_2D, 0);
    }
}

void GfxCommandListOpenGL::bindSampler(SamplerHandle sampler, uint32_t binding, uint32_t) {
    auto it = m_state->samplers.find(sampler.id);
    if (it != m_state->samplers.end()) {
        const GLSampler& glSampler = it->second;
        m_state->bindSampler(binding, glSampler.id);
    }
}

void GfxCommandListOpenGL::pushConstants(ShaderStage, const void* data, uint32_t size, uint32_t) {

    auto it = m_state->pipelines.find(m_currentPipeline.id);
    if (it == m_state->pipelines.end()) {
        return;
    }

    GLuint program = it->second.program;
    if (program == 0) {
        return;
    }

    glUseProgram(program);

    // Handle extended PerObjectUniforms struct with PBR (352 bytes = 88 floats)
    // Layout:
    // - viewProjection: mat4 (floats 0-15, bytes 0-63)
    // - model: mat4 (floats 16-31, bytes 64-127)
    // - normalMatrix: mat4 (floats 32-47, bytes 128-191)
    // - tintColor: vec4 (floats 48-51, bytes 192-207)
    // - useTexture: int32 (bytes 208-211)
    // - alphaCutoff: float (bytes 212-215)
    // - _pad1, _pad2: floats (bytes 216-223)
    // - uvRect: vec4 (floats 56-59, bytes 224-239)
    // PBR extensions:
    // - textureFlags: vec4 (floats 60-63, bytes 240-255)
    // - materialParams1: vec4 (floats 64-67, bytes 256-271)
    // - materialParams2: vec4 (floats 68-71, bytes 272-287)
    // - cameraPosition: vec4 (floats 72-75, bytes 288-303)
    // - albedoColor: vec4 (floats 76-79, bytes 304-319)
    // - emissiveColor: vec4 (floats 80-83, bytes 320-335)
    // - receiveShadow: int32 (bytes 336-339)
    // - _pad3, _pad4, _pad5: floats (bytes 340-351)
    if (size == sizeof(float) * 88) {
        const float* floatData = static_cast<const float*>(data);
        const uint8_t* byteData = static_cast<const uint8_t*>(data);

        GLint vpLoc = glGetUniformLocation(program, "u_ViewProjection");
        if (vpLoc != -1) {
            glUniformMatrix4fv(vpLoc, 1, GL_FALSE, &floatData[0]);
        }

        GLint modelLoc = glGetUniformLocation(program, "u_Model");
        if (modelLoc != -1) {
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &floatData[16]);
        }

        GLint normalLoc = glGetUniformLocation(program, "u_NormalMatrix");
        if (normalLoc != -1) {
            glUniformMatrix4fv(normalLoc, 1, GL_FALSE, &floatData[32]);
        }

        GLint tintLoc = glGetUniformLocation(program, "u_TintColor");
        if (tintLoc != -1) {
            glUniform4f(tintLoc, floatData[48], floatData[49], floatData[50], floatData[51]);
        }

        // useTexture is an int32 at byte offset 208
        GLint useTexLoc = glGetUniformLocation(program, "u_UseTexture");
        if (useTexLoc != -1) {
            int32_t useTexture = *reinterpret_cast<const int32_t*>(byteData + 208);
            glUniform1i(useTexLoc, useTexture);
        }

        // alphaCutoff is a float at byte offset 212 (float index 53)
        GLint alphaCutoffLoc = glGetUniformLocation(program, "u_AlphaCutoff");
        if (alphaCutoffLoc != -1) {
            glUniform1f(alphaCutoffLoc, floatData[53]);
        }

        // uvRect is vec4 at byte offset 224 (float indices 56-59)
        GLint uvRectLoc = glGetUniformLocation(program, "u_UVRect");
        if (uvRectLoc != -1) {
            glUniform4f(uvRectLoc, floatData[56], floatData[57], floatData[58], floatData[59]);
        }

        // PBR extensions
        // textureFlags at byte offset 240 (float indices 60-63)
        GLint texFlagsLoc = glGetUniformLocation(program, "u_TextureFlags");
        if (texFlagsLoc != -1) {
            glUniform4f(texFlagsLoc, floatData[60], floatData[61], floatData[62], floatData[63]);
        }

        // RoundedRect: u_CornerRadius uses same offset as textureFlags (240)
        GLint cornerRadiusLoc = glGetUniformLocation(program, "u_CornerRadius");
        if (cornerRadiusLoc != -1) {
            glUniform4f(cornerRadiusLoc, floatData[60], floatData[61], floatData[62], floatData[63]);
        }

        // Polygon: u_PolygonParams uses same offset as textureFlags (240)
        // x=sides, y=rotation, z=unused, w=unused
        GLint polygonParamsLoc = glGetUniformLocation(program, "u_PolygonParams");
        if (polygonParamsLoc != -1) {
            glUniform4f(polygonParamsLoc, floatData[60], floatData[61], floatData[62], floatData[63]);
        }

        // materialParams1 at byte offset 256 (float indices 64-67)
        GLint params1Loc = glGetUniformLocation(program, "u_MaterialParams1");
        if (params1Loc != -1) {
            glUniform4f(params1Loc, floatData[64], floatData[65], floatData[66], floatData[67]);
        }

        // RoundedRect: u_Size uses same offset as materialParams1 (256), but only xy components
        GLint sizeLoc = glGetUniformLocation(program, "u_Size");
        if (sizeLoc != -1) {
            glUniform2f(sizeLoc, floatData[64], floatData[65]);
        }

        // materialParams2 at byte offset 272 (float indices 68-71)
        GLint params2Loc = glGetUniformLocation(program, "u_MaterialParams2");
        if (params2Loc != -1) {
            glUniform4f(params2Loc, floatData[68], floatData[69], floatData[70], floatData[71]);
        }

        // cameraPosition at byte offset 288 (float indices 72-75)
        GLint camPosLoc = glGetUniformLocation(program, "u_CameraPosition");
        if (camPosLoc != -1) {
            glUniform4f(camPosLoc, floatData[72], floatData[73], floatData[74], floatData[75]);
        }

        // albedoColor at byte offset 304 (float indices 76-79)
        GLint albedoLoc = glGetUniformLocation(program, "u_AlbedoColor");
        if (albedoLoc != -1) {
            glUniform4f(albedoLoc, floatData[76], floatData[77], floatData[78], floatData[79]);
        }

        // emissiveColor at byte offset 320 (float indices 80-83)
        GLint emissiveLoc = glGetUniformLocation(program, "u_EmissiveColor");
        if (emissiveLoc != -1) {
            glUniform4f(emissiveLoc, floatData[80], floatData[81], floatData[82], floatData[83]);
        }

        // receiveShadow is an int32 at byte offset 336
        GLint receiveShadowLoc = glGetUniformLocation(program, "u_ReceiveShadow");
        if (receiveShadowLoc != -1) {
            int32_t receiveShadow = *reinterpret_cast<const int32_t*>(byteData + 336);
            glUniform1i(receiveShadowLoc, receiveShadow);
        }
    }
    // Handle legacy PerObjectUniforms struct (240 bytes = 60 floats worth of data)
    // Layout:
    // - viewProjection: mat4 (floats 0-15, bytes 0-63)
    // - model: mat4 (floats 16-31, bytes 64-127)
    // - normalMatrix: mat4 (floats 32-47, bytes 128-191)
    // - tintColor: vec4 (floats 48-51, bytes 192-207)
    // - useTexture: int32 (bytes 208-211)
    // - alphaCutoff: float (bytes 212-215)
    // - _pad1, _pad2: floats (bytes 216-223)
    // - uvRect: vec4 (bytes 224-239)
    else if (size == sizeof(float) * 60) {
        const float* floatData = static_cast<const float*>(data);
        const uint8_t* byteData = static_cast<const uint8_t*>(data);

        GLint vpLoc = glGetUniformLocation(program, "u_ViewProjection");
        if (vpLoc != -1) {
            glUniformMatrix4fv(vpLoc, 1, GL_FALSE, &floatData[0]);
        }

        GLint modelLoc = glGetUniformLocation(program, "u_Model");
        if (modelLoc != -1) {
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &floatData[16]);
        }

        GLint normalLoc = glGetUniformLocation(program, "u_NormalMatrix");
        if (normalLoc != -1) {
            glUniformMatrix4fv(normalLoc, 1, GL_FALSE, &floatData[32]);
        }

        GLint tintLoc = glGetUniformLocation(program, "u_TintColor");
        if (tintLoc != -1) {
            glUniform4f(tintLoc, floatData[48], floatData[49], floatData[50], floatData[51]);
        }

        // useTexture is an int32 at byte offset 208
        GLint useTexLoc = glGetUniformLocation(program, "u_UseTexture");
        if (useTexLoc != -1) {
            int32_t useTexture = *reinterpret_cast<const int32_t*>(byteData + 208);
            glUniform1i(useTexLoc, useTexture);
        }

        // alphaCutoff is a float at byte offset 212 (float index 53)
        GLint alphaCutoffLoc = glGetUniformLocation(program, "u_AlphaCutoff");
        if (alphaCutoffLoc != -1) {
            glUniform1f(alphaCutoffLoc, floatData[53]);
        }

        // uvRect is vec4 at byte offset 224 (float indices 56-59)
        GLint uvRectLoc = glGetUniformLocation(program, "u_UVRect");
        if (uvRectLoc != -1) {
            glUniform4f(uvRectLoc, floatData[56], floatData[57], floatData[58], floatData[59]);
        }
    }
    // Legacy 208-byte struct support (for backwards compatibility)
    // Also used by shadow map shaders which use u_LightSpaceMatrix instead of u_ViewProjection
    else if (size == sizeof(float) * 52) {
        const float* floatData = static_cast<const float*>(data);

        GLint vpLoc = glGetUniformLocation(program, "u_ViewProjection");
        if (vpLoc != -1) {
            glUniformMatrix4fv(vpLoc, 1, GL_FALSE, &floatData[0]);
        }

        // Shadow map shaders use u_LightSpaceMatrix instead of u_ViewProjection
        GLint lightSpaceLoc = glGetUniformLocation(program, "u_LightSpaceMatrix");
        if (lightSpaceLoc != -1) {
            glUniformMatrix4fv(lightSpaceLoc, 1, GL_FALSE, &floatData[0]);
        }

        GLint modelLoc = glGetUniformLocation(program, "u_Model");
        if (modelLoc != -1) {
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &floatData[16]);
        }

        GLint normalLoc = glGetUniformLocation(program, "u_NormalMatrix");
        if (normalLoc != -1) {
            glUniformMatrix4fv(normalLoc, 1, GL_FALSE, &floatData[32]);
        }

        GLint tintLoc = glGetUniformLocation(program, "u_TintColor");
        if (tintLoc != -1) {
            glUniform4f(tintLoc, floatData[48], floatData[49], floatData[50], floatData[51]);
        }
    }

    else if (size == sizeof(float) * 16) {
        GLint location = glGetUniformLocation(program, "u_ViewProjection");
        if (location != -1) {
            const float* matrix = static_cast<const float*>(data);
            glUniformMatrix4fv(location, 1, GL_FALSE, matrix);
        }
    }
}

GLint GfxCommandListOpenGL::getUniformLocationCached(GLPipeline& pipeline, const char* name) {
    auto cached = pipeline.uniformLocations.find(name);
    if (cached != pipeline.uniformLocations.end()) {
        return cached->second;
    }
    GLint location = glGetUniformLocation(pipeline.program, name);
    pipeline.uniformLocations.emplace(name, location);
    return location;
}

void GfxCommandListOpenGL::setUniformFloat(const char* name, float value) {
    auto it = m_state->pipelines.find(m_currentPipeline.id);
    if (it == m_state->pipelines.end()) return;

    if (it->second.program == 0) return;

    GLint location = getUniformLocationCached(it->second, name);
    if (location != -1) {
        glUniform1f(location, value);
    }
}

void GfxCommandListOpenGL::setUniformInt(const char* name, int value) {
    auto it = m_state->pipelines.find(m_currentPipeline.id);
    if (it == m_state->pipelines.end()) return;

    if (it->second.program == 0) return;

    GLint location = getUniformLocationCached(it->second, name);
    if (location != -1) {
        glUniform1i(location, value);
    }
}

void GfxCommandListOpenGL::setUniformBool(const char* name, bool value) {
    setUniformInt(name, value ? 1 : 0);
}

void GfxCommandListOpenGL::setUniformVec2(const char* name, const Vec2& value) {
    auto it = m_state->pipelines.find(m_currentPipeline.id);
    if (it == m_state->pipelines.end()) return;

    if (it->second.program == 0) return;

    GLint location = getUniformLocationCached(it->second, name);
    if (location != -1) {
        glUniform2f(location, value.x, value.y);
    }
}

void GfxCommandListOpenGL::setUniformVec3(const char* name, const Vec3& value) {
    auto it = m_state->pipelines.find(m_currentPipeline.id);
    if (it == m_state->pipelines.end()) return;

    if (it->second.program == 0) return;

    GLint location = getUniformLocationCached(it->second, name);
    if (location != -1) {
        glUniform3f(location, value.x, value.y, value.z);
    }
}

void GfxCommandListOpenGL::setUniformVec4(const char* name, const Vec4& value) {
    auto it = m_state->pipelines.find(m_currentPipeline.id);
    if (it == m_state->pipelines.end()) return;

    if (it->second.program == 0) return;

    GLint location = getUniformLocationCached(it->second, name);
    if (location != -1) {
        glUniform4f(location, value.x, value.y, value.z, value.w);
    }
}

void GfxCommandListOpenGL::setUniformMat4(const char* name, const math::Mat4& value) {
    auto it = m_state->pipelines.find(m_currentPipeline.id);
    if (it == m_state->pipelines.end()) return;

    if (it->second.program == 0) return;

    GLint location = getUniformLocationCached(it->second, name);
    if (location != -1) {
        glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(value.GetGLM()));
    }
}

void GfxCommandListOpenGL::setUniformMat4Array(const char* name, const math::Mat4* values, size_t count) {
    auto it = m_state->pipelines.find(m_currentPipeline.id);
    if (it == m_state->pipelines.end()) return;

    if (it->second.program == 0) return;

    GLint location = getUniformLocationCached(it->second, name);
    if (location != -1 && count > 0) {

        std::vector<float> matrixData;
        matrixData.reserve(count * 16);

        for (size_t i = 0; i < count; ++i) {
            const float* matPtr = glm::value_ptr(values[i].GetGLM());
            matrixData.insert(matrixData.end(), matPtr, matPtr + 16);
        }

        glUniformMatrix4fv(location, static_cast<GLsizei>(count), GL_FALSE, matrixData.data());
    }
}

void GfxCommandListOpenGL::setUniformColor(const char* name, const Color& value) {
    setUniformVec4(name, Vec4(value.r, value.g, value.b, value.a));
}

void GfxCommandListOpenGL::resetMaterialData() {
    // OpenGL doesn't use a material UBO ring buffer like Vulkan, so this is a no-op
    // Material data is set directly via uniforms for each draw call
}

void GfxCommandListOpenGL::draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) {
    auto it = m_state->pipelines.find(m_currentPipeline.id);
    if (it == m_state->pipelines.end()) {
        return;
    }

    GLenum topology = GLUtils::toGLTopology(it->second.topology);

    if (instanceCount > 1) {
        glDrawArraysInstancedBaseInstance(topology, firstVertex, vertexCount, instanceCount, firstInstance);
    } else {
        glDrawArrays(topology, firstVertex, vertexCount);
    }
}

void GfxCommandListOpenGL::drawIndexed(uint32_t indexCount, uint32_t instanceCount,
                                        uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) {
    auto it = m_state->pipelines.find(m_currentPipeline.id);
    if (it == m_state->pipelines.end()) {

        return;
    }

    GLenum topology = GLUtils::toGLTopology(it->second.topology);
    GLenum indexType = (m_currentIndexFormat == IndexFormat::UInt16) ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
    size_t indexSize = (m_currentIndexFormat == IndexFormat::UInt16) ? sizeof(uint16_t) : sizeof(uint32_t);

    void* indices = reinterpret_cast<void*>(m_indexBufferOffset + firstIndex * indexSize);

    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {

        GLint currentProgram = 0;
        glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);

        GLint maxTextureUnits = 0;
        glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &maxTextureUnits);

    }

    GLint currentVAO = 0;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &currentVAO);

    GLuint expectedVAO = 0;
    auto vaoIt = it->second.vaos.find(m_state->currentContext);
    if (vaoIt != it->second.vaos.end()) {
        expectedVAO = vaoIt->second;
    }

    GLint currentArrayBuffer = 0;
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &currentArrayBuffer);

    GLint currentElementBuffer = 0;
    glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &currentElementBuffer);

    if (instanceCount > 1) {
        if (vertexOffset != 0) {
            glDrawElementsInstancedBaseVertexBaseInstance(topology, indexCount, indexType, indices,
                                                         instanceCount, vertexOffset, firstInstance);
        } else {
            glDrawElementsInstancedBaseInstance(topology, indexCount, indexType, indices,
                                               instanceCount, firstInstance);
        }
    } else {
        if (vertexOffset != 0) {
            glDrawElementsBaseVertex(topology, indexCount, indexType, indices, vertexOffset);
        } else {

            glDrawElements(topology, indexCount, indexType, indices);

        }
    }

    error = glGetError();
    if (error != GL_NO_ERROR) {

    }
}

void GfxCommandListOpenGL::barrier() {

    glMemoryBarrier(GL_ALL_BARRIER_BITS);
}

void GfxCommandListOpenGL::beginDebugMarker(const char* name) {
#ifdef GL_KHR_debug
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, name);
#endif
}

void GfxCommandListOpenGL::endDebugMarker() {
#ifdef GL_KHR_debug
    glPopDebugGroup();
#endif
}

void GfxCommandListOpenGL::execute() {

}

}
