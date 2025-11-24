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
        glScissor(scissor.x, scissor.y, scissor.width, scissor.height);
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

    GLboolean scissorEnabled = glIsEnabled(GL_SCISSOR_TEST);
    GLboolean depthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
    GLboolean depthWriteEnabled;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depthWriteEnabled);
    GLint scissorBox[4];
    glGetIntegerv(GL_SCISSOR_BOX, scissorBox);
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    GLint drawFBO, readFBO;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFBO);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFBO);

    glClear(clearFlags);

    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {

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

                for (const auto& attr : pipeline.vertexLayout.attributes) {
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
                            pipeline.vertexLayout.stride,
                            reinterpret_cast<void*>(static_cast<uintptr_t>(attr.offset))
                        );

                        GLenum error = glGetError();
                        if (error != GL_NO_ERROR) {

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

void GfxCommandListOpenGL::bindUniformBuffer(UniformBufferHandle buffer, uint32_t binding, uint32_t set) {
    auto it = m_state->uniformBuffers.find(buffer.id);
    if (it != m_state->uniformBuffers.end()) {
        const GLUniformBuffer& glBuffer = it->second;
        glBindBufferBase(GL_UNIFORM_BUFFER, binding, glBuffer.id);
    }
}

void GfxCommandListOpenGL::bindTexture(TextureHandle texture, uint32_t binding, uint32_t set) {
    auto it = m_state->textures.find(texture.id);
    if (it != m_state->textures.end()) {
        const GLTexture& glTexture = it->second;

        m_state->bindTexture(binding, glTexture.target, glTexture.id);
    } else {

    }
}

void GfxCommandListOpenGL::bindSampler(SamplerHandle sampler, uint32_t binding, uint32_t set) {
    auto it = m_state->samplers.find(sampler.id);
    if (it != m_state->samplers.end()) {
        const GLSampler& glSampler = it->second;
        m_state->bindSampler(binding, glSampler.id);
    }
}

void GfxCommandListOpenGL::pushConstants(ShaderStage stage, const void* data, uint32_t size, uint32_t offset) {

    auto it = m_state->pipelines.find(m_currentPipeline.id);
    if (it == m_state->pipelines.end()) {
        return;
    }

    GLuint program = it->second.program;
    if (program == 0) {
        return;
    }

    glUseProgram(program);

    if (size == sizeof(float) * 52) {
        const float* floatData = static_cast<const float*>(data);

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
    }

    else if (size == sizeof(float) * 16) {
        GLint location = glGetUniformLocation(program, "u_ViewProjection");
        if (location != -1) {
            const float* matrix = static_cast<const float*>(data);
            glUniformMatrix4fv(location, 1, GL_FALSE, matrix);
        }
    }
}

void GfxCommandListOpenGL::setUniformFloat(const char* name, float value) {
    auto it = m_state->pipelines.find(m_currentPipeline.id);
    if (it == m_state->pipelines.end()) return;

    GLuint program = it->second.program;
    if (program == 0) return;

    GLint location = glGetUniformLocation(program, name);
    if (location != -1) {
        glUniform1f(location, value);
    }
}

void GfxCommandListOpenGL::setUniformInt(const char* name, int value) {
    auto it = m_state->pipelines.find(m_currentPipeline.id);
    if (it == m_state->pipelines.end()) return;

    GLuint program = it->second.program;
    if (program == 0) return;

    GLint location = glGetUniformLocation(program, name);
    if (location != -1) {
        glUniform1i(location, value);

        GLenum error = glGetError();
        if (error != GL_NO_ERROR) {

        }
    } else {

    }
}

void GfxCommandListOpenGL::setUniformBool(const char* name, bool value) {
    setUniformInt(name, value ? 1 : 0);
}

void GfxCommandListOpenGL::setUniformVec2(const char* name, const Vec2& value) {
    auto it = m_state->pipelines.find(m_currentPipeline.id);
    if (it == m_state->pipelines.end()) return;

    GLuint program = it->second.program;
    if (program == 0) return;

    GLint location = glGetUniformLocation(program, name);
    if (location != -1) {
        glUniform2f(location, value.x, value.y);
    }
}

void GfxCommandListOpenGL::setUniformVec3(const char* name, const Vec3& value) {
    auto it = m_state->pipelines.find(m_currentPipeline.id);
    if (it == m_state->pipelines.end()) return;

    GLuint program = it->second.program;
    if (program == 0) return;

    GLint location = glGetUniformLocation(program, name);
    if (location != -1) {
        glUniform3f(location, value.x, value.y, value.z);
    }
}

void GfxCommandListOpenGL::setUniformVec4(const char* name, const Vec4& value) {
    auto it = m_state->pipelines.find(m_currentPipeline.id);
    if (it == m_state->pipelines.end()) return;

    GLuint program = it->second.program;
    if (program == 0) return;

    GLint location = glGetUniformLocation(program, name);
    if (location != -1) {
        glUniform4f(location, value.x, value.y, value.z, value.w);
    }
}

void GfxCommandListOpenGL::setUniformMat4(const char* name, const math::Mat4& value) {
    auto it = m_state->pipelines.find(m_currentPipeline.id);
    if (it == m_state->pipelines.end()) return;

    GLuint program = it->second.program;
    if (program == 0) return;

    GLint location = glGetUniformLocation(program, name);
    if (location != -1) {
        glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(value.GetGLM()));
    }
}

void GfxCommandListOpenGL::setUniformMat4Array(const char* name, const math::Mat4* values, size_t count) {
    auto it = m_state->pipelines.find(m_currentPipeline.id);
    if (it == m_state->pipelines.end()) return;

    GLuint program = it->second.program;
    if (program == 0) return;

    GLint location = glGetUniformLocation(program, name);
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
