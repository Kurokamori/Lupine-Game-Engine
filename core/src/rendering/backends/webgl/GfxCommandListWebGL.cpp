#ifdef __EMSCRIPTEN__

#include "lupine/rendering/backends/webgl/GfxCommandListWebGL.hpp"
#include "lupine/rendering/backends/webgl/WebGLState.hpp"
#include "lupine/logger/Logger.hpp"

#include <GLES3/gl3.h>
#include <glm/gtc/type_ptr.hpp>
#include <set>

namespace lupine {

GfxCommandListWebGL::GfxCommandListWebGL(WebGLState* state)
    : m_state(state) {
    m_vertexBuffers.resize(8);
}

GfxCommandListWebGL::~GfxCommandListWebGL() {
}

void GfxCommandListWebGL::setRenderTarget(RenderTargetHandle target) {
    m_currentRenderTarget = target;

    auto it = m_state->renderTargets.find(target.id);
    if (it != m_state->renderTargets.end()) {
        const WebGLRenderTarget& rt = it->second;
        m_state->bindFramebuffer(rt.fbo);

        // Track which textures are attached to this FBO to prevent feedback loops
        // When we draw, we'll check if any bound textures match these and unbind them
        m_state->currentFboColorTexture = rt.colorTexture;
        m_state->currentFboDepthTexture = rt.depthStencilTexture;

        if (rt.fbo == 0) {
            // Default framebuffer (canvas)
            // WebGL 2.0 doesn't need explicit draw/read buffer for default FBO
            m_state->currentFboColorTexture = 0;
            m_state->currentFboDepthTexture = 0;
        } else {
            // Custom framebuffer
            if (rt.colorTexture == 0) {
                // Depth-only render target
                GLenum drawBuffers[] = { GL_NONE };
                glDrawBuffers(1, drawBuffers);
            } else {
                GLenum drawBuffers[] = { GL_COLOR_ATTACHMENT0 };
                glDrawBuffers(1, drawBuffers);
            }
        }
    }
}

void GfxCommandListWebGL::setViewport(const Viewport& viewport) {
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

void GfxCommandListWebGL::setScissor(const ScissorRect& scissor) {
    if (scissor.width > 0 && scissor.height > 0) {
        glEnable(GL_SCISSOR_TEST);
        // ScissorRect is top-left origin framebuffer pixels (Viewport/DX/Vulkan
        // contract); glScissor is bottom-left origin, so flip Y using the active
        // viewport (covers the full render target in this engine).
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

void GfxCommandListWebGL::clearColor(const Color& color) {
    glClearColor(color.r, color.g, color.b, color.a);
    m_pendingColorClear = true;
}

void GfxCommandListWebGL::clearDepth(float depth) {
    // Ensure depth writing is enabled before clearing
    glDepthMask(GL_TRUE);
    glClearDepthf(depth);

    GLbitfield clearFlags = GL_DEPTH_BUFFER_BIT;
    if (m_pendingColorClear) {
        clearFlags |= GL_COLOR_BUFFER_BIT;
        m_pendingColorClear = false;
    }

    glClear(clearFlags);
}

void GfxCommandListWebGL::clearStencil(uint8_t stencil) {
    glClearStencil(stencil);
    glClear(GL_STENCIL_BUFFER_BIT);
}

void GfxCommandListWebGL::bindPipeline(PipelineHandle pipeline) {
    m_currentPipeline = pipeline;

    auto it = m_state->pipelines.find(pipeline.id);
    if (it == m_state->pipelines.end()) {
        return;
    }

    WebGLPipeline& glPipeline = const_cast<WebGLPipeline&>(it->second);

    // Bind shader program
    m_state->bindProgram(glPipeline.program);

    // Bind VAO
    m_state->bindVAO(glPipeline.vao);

    // Set blend state
    if (glPipeline.blendState.blendEnable) {
        glEnable(GL_BLEND);
        glBlendFuncSeparate(
            WebGLUtils::toWebGLBlendFactor(glPipeline.blendState.srcColorBlend),
            WebGLUtils::toWebGLBlendFactor(glPipeline.blendState.dstColorBlend),
            WebGLUtils::toWebGLBlendFactor(glPipeline.blendState.srcAlphaBlend),
            WebGLUtils::toWebGLBlendFactor(glPipeline.blendState.dstAlphaBlend)
        );
        glBlendEquationSeparate(
            WebGLUtils::toWebGLBlendOp(glPipeline.blendState.colorBlendOp),
            WebGLUtils::toWebGLBlendOp(glPipeline.blendState.alphaBlendOp)
        );
    } else {
        glDisable(GL_BLEND);
    }

    // Set depth state
    if (glPipeline.depthStencilState.depthTestEnable) {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(WebGLUtils::toWebGLCompareFunc(glPipeline.depthStencilState.depthCompareFunc));
    } else {
        glDisable(GL_DEPTH_TEST);
    }

    glDepthMask(glPipeline.depthStencilState.depthWriteEnable ? GL_TRUE : GL_FALSE);

    // Set stencil state
    if (glPipeline.depthStencilState.stencilEnable) {
        glEnable(GL_STENCIL_TEST);
        glStencilFunc(GL_ALWAYS, 0, 0xFF);
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
        glStencilMask(0xFF);
    } else {
        glDisable(GL_STENCIL_TEST);
    }

    // Set rasterizer state
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

    // WebGL doesn't support GL_LINE polygon mode for fill mode
    // Wireframe rendering must be done differently in WebGL:
    // 1. Use line primitives (GL_LINES, GL_LINE_STRIP, GL_LINE_LOOP) with the wireframe shader
    // 2. Or generate line geometry from triangles on the CPU
    // 3. Or use a shader-based approach (barycentric coordinates)
    // The engine provides a wireframe shader that can be used with line primitives.
    if (glPipeline.rasterizerState.fillMode == FillMode::Wireframe) {
        
    }

    // Set depth bias if enabled
    if (glPipeline.rasterizerState.depthBiasEnable) {
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(
            glPipeline.rasterizerState.depthBiasSlopeFactor,
            glPipeline.rasterizerState.depthBiasConstantFactor
        );
    } else {
        glDisable(GL_POLYGON_OFFSET_FILL);
    }
}

void GfxCommandListWebGL::setLineWidth(float width) {
    // WebGL only supports line width of 1.0
    // glLineWidth is available but browsers typically ignore values other than 1.0
    glLineWidth(width);
}

void GfxCommandListWebGL::bindVertexBuffer(BufferHandle buffer, uint32_t binding, uint64_t offset) {
    if (binding >= m_vertexBuffers.size()) {
        m_vertexBuffers.resize(binding + 1);
    }

    m_vertexBuffers[binding].buffer = buffer;
    m_vertexBuffers[binding].offset = offset;

    auto bufferIt = m_state->buffers.find(buffer.id);
    if (bufferIt == m_state->buffers.end()) {
        return;
    }

    const WebGLBuffer& glBuffer = bufferIt->second;
    if (glBuffer.id == 0) {
        return;
    }

    m_state->bindBuffer(GL_ARRAY_BUFFER, glBuffer.id);

    // Set up vertex attributes if pipeline is bound
    auto pipelineIt = m_state->pipelines.find(m_currentPipeline.id);
    if (pipelineIt != m_state->pipelines.end()) {
        WebGLPipeline& pipeline = const_cast<WebGLPipeline&>(pipelineIt->second);

        // Clear any previous GL errors
        while (glGetError() != GL_NO_ERROR);

        // Track which attributes are bound from vertex data
        std::set<std::string> boundAttributes;

        // Select the layout that owns this binding: binding 0 is the primary
        // geometry layout; higher bindings come from the per-instance / extra
        // vertex buffers. Attributes are set up against the currently bound
        // GL_ARRAY_BUFFER and tagged with the appropriate attribute divisor
        // (1 for per-instance data).
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
                    GLenum type = WebGLUtils::getVertexFormatType(attr.format);
                    GLint count = WebGLUtils::getVertexFormatCount(attr.format);
                    GLboolean normalized = WebGLUtils::isVertexFormatNormalized(attr.format) ? GL_TRUE : GL_FALSE;

                    glEnableVertexAttribArray(location);
                    glVertexAttribPointer(
                        location,
                        count,
                        type,
                        normalized,
                        layout->stride,
                        reinterpret_cast<void*>(static_cast<uintptr_t>(attr.offset + offset))
                    );
                    glVertexAttribDivisor(location, divisor);
                    boundAttributes.insert(attr.name);
                }
            }
        }

        // Set default values for common attributes that aren't in the vertex
        // layout. Only applies to the primary (binding 0) geometry buffer; this
        // prevents meshes from being invisible when they don't have vertex colors.
        if (binding == 0) {
            if (boundAttributes.find("a_Color") == boundAttributes.end()) {
                GLint colorLoc = glGetAttribLocation(pipeline.program, "a_Color");
                if (colorLoc >= 0) {
                    glDisableVertexAttribArray(colorLoc);
                    glVertexAttrib4f(colorLoc, 1.0f, 1.0f, 1.0f, 1.0f); // Default white color
                }
            }

            // Default tangent to (1, 0, 0) if not provided
            if (boundAttributes.find("a_Tangent") == boundAttributes.end()) {
                GLint tangentLoc = glGetAttribLocation(pipeline.program, "a_Tangent");
                if (tangentLoc >= 0) {
                    glDisableVertexAttribArray(tangentLoc);
                    glVertexAttrib3f(tangentLoc, 1.0f, 0.0f, 0.0f);
                }
            }

            // Default bitangent to (0, 1, 0) if not provided
            if (boundAttributes.find("a_Bitangent") == boundAttributes.end()) {
                GLint bitangentLoc = glGetAttribLocation(pipeline.program, "a_Bitangent");
                if (bitangentLoc >= 0) {
                    glDisableVertexAttribArray(bitangentLoc);
                    glVertexAttrib3f(bitangentLoc, 0.0f, 1.0f, 0.0f);
                }
            }
        }

        pipeline.vaoInitialized = true;
    }
}

void GfxCommandListWebGL::bindIndexBuffer(BufferHandle buffer, IndexFormat format, uint64_t offset) {
    m_currentIndexBuffer = buffer;
    m_currentIndexFormat = format;
    m_indexBufferOffset = offset;

    auto it = m_state->buffers.find(buffer.id);
    if (it != m_state->buffers.end()) {
        const WebGLBuffer& glBuffer = it->second;
        if (glBuffer.id != 0) {
            m_state->bindBuffer(GL_ELEMENT_ARRAY_BUFFER, glBuffer.id);
        }
    }
}

void GfxCommandListWebGL::bindUniformBuffer(UniformBufferHandle buffer, uint32_t binding, uint32_t set) {
    auto it = m_state->uniformBuffers.find(buffer.id);
    if (it != m_state->uniformBuffers.end()) {
        const WebGLUniformBuffer& glBuffer = it->second;
        glBindBufferBase(GL_UNIFORM_BUFFER, binding, glBuffer.id);
    }
}

void GfxCommandListWebGL::bindTexture(TextureHandle texture, uint32_t binding, uint32_t set) {
    auto it = m_state->textures.find(texture.id);
    if (it != m_state->textures.end()) {
        const WebGLTexture& glTexture = it->second;
        m_state->bindTexture(binding, glTexture.target, glTexture.id);

        // DEBUG: Log successful texture binding
        static int bindLogCount = 0;
        if (bindLogCount < 3) {
            
            bindLogCount++;
        }
    } else {
        // Unbind texture slot when handle is invalid
        m_state->bindTexture(binding, GL_TEXTURE_2D, 0);

        // DEBUG: Log texture not found
        static int notFoundLogCount = 0;
        if (notFoundLogCount < 3 && texture.id != 0) {
            LOG_WARN(LogCategory::Render, "WebGL: Texture handle id={} NOT FOUND in textures map!", texture.id);
            notFoundLogCount++;
        }
    }
}

void GfxCommandListWebGL::bindSampler(SamplerHandle sampler, uint32_t binding, uint32_t set) {
    auto it = m_state->samplers.find(sampler.id);
    if (it != m_state->samplers.end()) {
        const WebGLSampler& glSampler = it->second;
        m_state->bindSampler(binding, glSampler.id);
    }
}

void GfxCommandListWebGL::pushConstants(ShaderStage stage, const void* data, uint32_t size, uint32_t offset) {
    auto it = m_state->pipelines.find(m_currentPipeline.id);
    if (it == m_state->pipelines.end()) {
        return;
    }

    GLuint program = it->second.program;
    if (program == 0) {
        return;
    }

    glUseProgram(program);

    // Handle extended PerObjectUniforms struct (352 bytes = 88 floats worth of data)
    // Layout:
    // - u_ViewProjection: offset 0 (mat4, 64 bytes, floats 0-15)
    // - u_Model: offset 64 (mat4, 64 bytes, floats 16-31)
    // - u_NormalMatrix: offset 128 (mat4, 64 bytes, floats 32-47)
    // - u_TintColor: offset 192 (vec4, 16 bytes, floats 48-51)
    // - u_UseTexture: offset 208 (int, 4 bytes)
    // - u_AlphaCutoff: offset 212 (float, 4 bytes, float 53)
    // - _pad1, _pad2: offset 216-223 (padding)
    // - u_UVRect: offset 224 (vec4, 16 bytes, floats 56-59)
    // - u_TextureFlags: offset 240 (vec4, 16 bytes, floats 60-63)
    // - u_MaterialParams1: offset 256 (vec4, 16 bytes, floats 64-67)
    // - u_MaterialParams2: offset 272 (vec4, 16 bytes, floats 68-71)
    // - u_CameraPosition: offset 288 (vec4, 16 bytes, floats 72-75)
    // - u_AlbedoColor: offset 304 (vec4, 16 bytes, floats 76-79)
    // - u_EmissiveColor: offset 320 (vec4, 16 bytes, floats 80-83)
    // - u_ReceiveShadow: offset 336 (int, 4 bytes)
    // - _pad3, _pad4, _pad5: offset 340-351 (padding)
    // Total: 352 bytes
    if (size == 352) {
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
        setUniformSafe4f(tintLoc, floatData[48], floatData[49], floatData[50], floatData[51]);

        GLint useTexLoc = glGetUniformLocation(program, "u_UseTexture");
        int32_t useTexture = *reinterpret_cast<const int32_t*>(byteData + 208);
        if (useTexLoc != -1) {
            glUniform1i(useTexLoc, useTexture);
        }

        // PBR shaders use u_UseAlbedoMap instead of u_UseTexture
        // Set both to ensure textures work across all shader types
        GLint useAlbedoMapLoc = glGetUniformLocation(program, "u_UseAlbedoMap");
        if (useAlbedoMapLoc != -1) {
            glUniform1i(useAlbedoMapLoc, useTexture);
        }

        GLint alphaCutoffLoc = glGetUniformLocation(program, "u_AlphaCutoff");
        if (alphaCutoffLoc != -1) {
            glUniform1f(alphaCutoffLoc, floatData[53]);
        }

        GLint uvRectLoc = glGetUniformLocation(program, "u_UVRect");
        setUniformSafe4f(uvRectLoc, floatData[56], floatData[57], floatData[58], floatData[59]);

        // PBR extensions
        GLint texFlagsLoc = glGetUniformLocation(program, "u_TextureFlags");
        setUniformSafe4f(texFlagsLoc, floatData[60], floatData[61], floatData[62], floatData[63]);

        // PBR shaders use individual u_Use*Map uniforms instead of u_TextureFlags vec4
        // textureFlags: x=albedo, y=metallicRoughness, z=normal, w=emissive
        GLint useNormalMapLoc = glGetUniformLocation(program, "u_UseNormalMap");
        if (useNormalMapLoc != -1) {
            glUniform1i(useNormalMapLoc, floatData[62] > 0.5f ? 1 : 0);
        }
        GLint useMetallicRoughnessMapLoc = glGetUniformLocation(program, "u_UseMetallicRoughnessMap");
        if (useMetallicRoughnessMapLoc != -1) {
            glUniform1i(useMetallicRoughnessMapLoc, floatData[61] > 0.5f ? 1 : 0);
        }
        GLint useEmissiveMapLoc = glGetUniformLocation(program, "u_UseEmissiveMap");
        if (useEmissiveMapLoc != -1) {
            glUniform1i(useEmissiveMapLoc, floatData[63] > 0.5f ? 1 : 0);
        }
        GLint useAOMapLoc = glGetUniformLocation(program, "u_UseAOMap");
        if (useAOMapLoc != -1) {
            // AO is typically packed with metallic-roughness or separate
            glUniform1i(useAOMapLoc, 0);  // Default to no AO map
        }

        GLint matParams1Loc = glGetUniformLocation(program, "u_MaterialParams1");
        setUniformSafe4f(matParams1Loc, floatData[64], floatData[65], floatData[66], floatData[67]);

        // RoundedRect shader uniforms - reuse same offsets as textureFlags/materialParams1
        // u_CornerRadius uses same offset as textureFlags (offset 240, floats 60-63)
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

        // u_Size uses same offset as materialParams1 (offset 256, floats 64-65)
        GLint sizeLoc = glGetUniformLocation(program, "u_Size");
        if (sizeLoc != -1) {
            glUniform2f(sizeLoc, floatData[64], floatData[65]);
        }

        GLint matParams2Loc = glGetUniformLocation(program, "u_MaterialParams2");
        setUniformSafe4f(matParams2Loc, floatData[68], floatData[69], floatData[70], floatData[71]);

        // u_CameraPosition may be vec3 or vec4 depending on shader - use safe setter
        GLint camPosLoc = glGetUniformLocation(program, "u_CameraPosition");
        setUniformSafe3f(camPosLoc, floatData[72], floatData[73], floatData[74]);

        GLint albedoColorLoc = glGetUniformLocation(program, "u_AlbedoColor");
        setUniformSafe4f(albedoColorLoc, floatData[76], floatData[77], floatData[78], floatData[79]);

        GLint emissiveColorLoc = glGetUniformLocation(program, "u_EmissiveColor");
        setUniformSafe4f(emissiveColorLoc, floatData[80], floatData[81], floatData[82], floatData[83]);

        GLint receiveShadowLoc = glGetUniformLocation(program, "u_ReceiveShadow");
        if (receiveShadowLoc != -1) {
            int32_t receiveShadow = *reinterpret_cast<const int32_t*>(byteData + 336);
            glUniform1i(receiveShadowLoc, receiveShadow);
        }
    }
    // Handle legacy PerObjectUniforms struct (240 bytes = 60 floats worth of data)
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
        setUniformSafe4f(tintLoc, floatData[48], floatData[49], floatData[50], floatData[51]);

        GLint useTexLoc = glGetUniformLocation(program, "u_UseTexture");
        int32_t useTexture = *reinterpret_cast<const int32_t*>(byteData + 208);
        if (useTexLoc != -1) {
            glUniform1i(useTexLoc, useTexture);
        }

        // PBR shaders use u_UseAlbedoMap instead of u_UseTexture
        GLint useAlbedoMapLoc = glGetUniformLocation(program, "u_UseAlbedoMap");
        if (useAlbedoMapLoc != -1) {
            glUniform1i(useAlbedoMapLoc, useTexture);
        }

        GLint alphaCutoffLoc = glGetUniformLocation(program, "u_AlphaCutoff");
        if (alphaCutoffLoc != -1) {
            glUniform1f(alphaCutoffLoc, floatData[53]);
        }

        GLint uvRectLoc = glGetUniformLocation(program, "u_UVRect");
        setUniformSafe4f(uvRectLoc, floatData[56], floatData[57], floatData[58], floatData[59]);
    }
    // Legacy 208-byte struct support
    else if (size == sizeof(float) * 52) {
        const float* floatData = static_cast<const float*>(data);

        GLint vpLoc = glGetUniformLocation(program, "u_ViewProjection");
        if (vpLoc != -1) {
            glUniformMatrix4fv(vpLoc, 1, GL_FALSE, &floatData[0]);
        }

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
        setUniformSafe4f(tintLoc, floatData[48], floatData[49], floatData[50], floatData[51]);
    }
    // Single matrix
    else if (size == sizeof(float) * 16) {
        GLint location = glGetUniformLocation(program, "u_ViewProjection");
        if (location != -1) {
            const float* matrix = static_cast<const float*>(data);
            glUniformMatrix4fv(location, 1, GL_FALSE, matrix);
        }
    }
}

GLint GfxCommandListWebGL::getUniformLocation(const char* name) {
    auto pipelineIt = m_state->pipelines.find(m_currentPipeline.id);
    if (pipelineIt == m_state->pipelines.end()) {
        return -1;
    }

    WebGLPipeline& pipeline = const_cast<WebGLPipeline&>(pipelineIt->second);

    // Check cache first
    auto locIt = pipeline.uniformLocations.find(name);
    if (locIt != pipeline.uniformLocations.end()) {
        return locIt->second;
    }

    // Query and cache
    GLint location = glGetUniformLocation(pipeline.program, name);
    pipeline.uniformLocations[name] = location;
    return location;
}

// Type-safe uniform setters that query uniform type before setting
// This prevents "Uniform size does not match uniform method" errors

bool GfxCommandListWebGL::setUniformSafe4f(GLint location, float x, float y, float z, float w) {
    if (location == -1) return false;

    auto pipelineIt = m_state->pipelines.find(m_currentPipeline.id);
    if (pipelineIt == m_state->pipelines.end()) return false;

    GLuint program = pipelineIt->second.program;

    // Invalidate cache if program changed
    if (m_cachedProgram != program) {
        m_uniformTypeCache.clear();
        m_cachedProgram = program;
    }

    // Check cache for uniform type
    auto typeIt = m_uniformTypeCache.find(location);
    GLenum type;

    if (typeIt != m_uniformTypeCache.end()) {
        type = typeIt->second;
    } else {
        // Query uniform info
        GLint size;
        GLsizei length;
        char nameBuf[256];
        glGetActiveUniform(program, static_cast<GLuint>(location), 256, &length, &size, &type, nameBuf);

        // Note: glGetActiveUniform uses index, not location. We need to iterate.
        // For now, just query by iterating through all uniforms to find the matching location.
        GLint numUniforms = 0;
        glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &numUniforms);

        bool found = false;
        for (GLint i = 0; i < numUniforms; ++i) {
            glGetActiveUniform(program, static_cast<GLuint>(i), 256, &length, &size, &type, nameBuf);
            GLint loc = glGetUniformLocation(program, nameBuf);
            if (loc == location) {
                m_uniformTypeCache[location] = type;
                found = true;
                break;
            }
        }

        if (!found) {
            // Uniform not found - just try to set it (will fail silently if wrong type)
            glUniform4f(location, x, y, z, w);
            return true;
        }
    }

    // Set uniform based on actual type
    switch (type) {
        case GL_FLOAT_VEC4:
            glUniform4f(location, x, y, z, w);
            return true;
        case GL_FLOAT_VEC3:
            glUniform3f(location, x, y, z);
            return true;
        case GL_FLOAT_VEC2:
            glUniform2f(location, x, y);
            return true;
        case GL_FLOAT:
            glUniform1f(location, x);
            return true;
        default:
            // Type mismatch - log and skip
            return false;
    }
}

bool GfxCommandListWebGL::setUniformSafe3f(GLint location, float x, float y, float z) {
    if (location == -1) return false;

    auto pipelineIt = m_state->pipelines.find(m_currentPipeline.id);
    if (pipelineIt == m_state->pipelines.end()) return false;

    GLuint program = pipelineIt->second.program;

    if (m_cachedProgram != program) {
        m_uniformTypeCache.clear();
        m_cachedProgram = program;
    }

    auto typeIt = m_uniformTypeCache.find(location);
    GLenum type;

    if (typeIt != m_uniformTypeCache.end()) {
        type = typeIt->second;
    } else {
        GLint numUniforms = 0;
        glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &numUniforms);

        GLint size;
        GLsizei length;
        char nameBuf[256];
        bool found = false;

        for (GLint i = 0; i < numUniforms; ++i) {
            glGetActiveUniform(program, static_cast<GLuint>(i), 256, &length, &size, &type, nameBuf);
            GLint loc = glGetUniformLocation(program, nameBuf);
            if (loc == location) {
                m_uniformTypeCache[location] = type;
                found = true;
                break;
            }
        }

        if (!found) {
            glUniform3f(location, x, y, z);
            return true;
        }
    }

    switch (type) {
        case GL_FLOAT_VEC4:
            glUniform4f(location, x, y, z, 0.0f);
            return true;
        case GL_FLOAT_VEC3:
            glUniform3f(location, x, y, z);
            return true;
        case GL_FLOAT_VEC2:
            glUniform2f(location, x, y);
            return true;
        case GL_FLOAT:
            glUniform1f(location, x);
            return true;
        default:
            return false;
    }
}

bool GfxCommandListWebGL::setUniformSafe1f(GLint location, float value) {
    if (location == -1) return false;
    glUniform1f(location, value);
    return true;
}

bool GfxCommandListWebGL::setUniformSafe1i(GLint location, int value) {
    if (location == -1) return false;
    glUniform1i(location, value);
    return true;
}

bool GfxCommandListWebGL::setUniformSafeMatrix4fv(GLint location, const float* value) {
    if (location == -1) return false;
    glUniformMatrix4fv(location, 1, GL_FALSE, value);
    return true;
}

void GfxCommandListWebGL::setUniformFloat(const char* name, float value) {
    auto it = m_state->pipelines.find(m_currentPipeline.id);
    if (it == m_state->pipelines.end()) return;

    GLint location = getUniformLocation(name);
    if (location != -1) {
        glUniform1f(location, value);
    }
}

void GfxCommandListWebGL::setUniformInt(const char* name, int value) {
    auto it = m_state->pipelines.find(m_currentPipeline.id);
    if (it == m_state->pipelines.end()) return;

    GLint location = getUniformLocation(name);
    if (location != -1) {
        glUniform1i(location, value);

        // DEBUG: Log texture sampler uniform settings
        static int samplerLogCount = 0;
        if (samplerLogCount < 10 && (strcmp(name, "u_Texture") == 0 || strcmp(name, "u_AlbedoTexture") == 0 || strcmp(name, "u_AlbedoMap") == 0)) {
            
            samplerLogCount++;
        }
    } else {
        // DEBUG: Log when texture sampler uniform is NOT found (separate counters per uniform name)
        static int notFoundAlbedoTexCount = 0;
        static int notFoundTextureCount = 0;
        static int notFoundAlbedoMapCount = 0;
        if (strcmp(name, "u_AlbedoTexture") == 0 && notFoundAlbedoTexCount < 3) {
            LOG_WARN(LogCategory::Render, "WebGL: Sampler uniform 'u_AlbedoTexture' NOT FOUND (pipeline={})", m_currentPipeline.id);
            notFoundAlbedoTexCount++;
        } else if (strcmp(name, "u_Texture") == 0 && notFoundTextureCount < 3) {
            LOG_WARN(LogCategory::Render, "WebGL: Sampler uniform 'u_Texture' NOT FOUND (pipeline={})", m_currentPipeline.id);
            notFoundTextureCount++;
        } else if (strcmp(name, "u_AlbedoMap") == 0 && notFoundAlbedoMapCount < 3) {
            LOG_WARN(LogCategory::Render, "WebGL: Sampler uniform 'u_AlbedoMap' NOT FOUND (pipeline={})", m_currentPipeline.id);
            notFoundAlbedoMapCount++;
        }
    }
}

void GfxCommandListWebGL::setUniformBool(const char* name, bool value) {
    setUniformInt(name, value ? 1 : 0);
}

void GfxCommandListWebGL::setUniformVec2(const char* name, const Vec2& value) {
    auto it = m_state->pipelines.find(m_currentPipeline.id);
    if (it == m_state->pipelines.end()) return;

    GLint location = getUniformLocation(name);
    if (location != -1) {
        glUniform2f(location, value.x, value.y);
    }
}

void GfxCommandListWebGL::setUniformVec3(const char* name, const Vec3& value) {
    auto it = m_state->pipelines.find(m_currentPipeline.id);
    if (it == m_state->pipelines.end()) return;

    GLint location = getUniformLocation(name);
    if (location != -1) {
        glUniform3f(location, value.x, value.y, value.z);
    }
}

void GfxCommandListWebGL::setUniformVec4(const char* name, const Vec4& value) {
    auto it = m_state->pipelines.find(m_currentPipeline.id);
    if (it == m_state->pipelines.end()) return;

    GLint location = getUniformLocation(name);
    if (location != -1) {
        glUniform4f(location, value.x, value.y, value.z, value.w);
    }
}

void GfxCommandListWebGL::setUniformMat4(const char* name, const math::Mat4& value) {
    auto it = m_state->pipelines.find(m_currentPipeline.id);
    if (it == m_state->pipelines.end()) return;

    GLint location = getUniformLocation(name);
    if (location != -1) {
        glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(value.GetGLM()));
    }
}

void GfxCommandListWebGL::setUniformMat4Array(const char* name, const math::Mat4* values, size_t count) {
    auto it = m_state->pipelines.find(m_currentPipeline.id);
    if (it == m_state->pipelines.end()) return;

    GLint location = getUniformLocation(name);
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

void GfxCommandListWebGL::setUniformColor(const char* name, const Color& value) {
    setUniformVec4(name, Vec4(value.r, value.g, value.b, value.a));
}

void GfxCommandListWebGL::resetMaterialData() {
    // WebGL doesn't use a material UBO ring buffer like some other backends
    // Material data is set directly via uniforms
}

void GfxCommandListWebGL::reportDrawError() {
    // Only worth paying for during startup; a GL error on draw is a hard bug that shows up
    // immediately, and glGetError() costs a round trip in WebGL.
    if (m_drawErrorChecksExhausted) {
        return;
    }
    if (++m_drawsChecked > 2000) {
        m_drawErrorChecksExhausted = true;
        return;
    }

    GLenum error = glGetError();
    if (error == GL_NO_ERROR) {
        return;
    }

    auto it = m_state->pipelines.find(m_currentPipeline.id);
    if (it == m_state->pipelines.end()) {
        LOG_ERROR(LogCategory::Render, "WebGL draw error 0x{:x} (no pipeline bound)", error);
        return;
    }

    const GLuint program = it->second.program;

    // One report per program is enough to identify the culprit.
    if (!m_reportedErrorPrograms.insert(program).second) {
        return;
    }

    LOG_ERROR(LogCategory::Render,
              "WebGL draw error 0x{:x} on pipeline id={} (program={}). Sampler assignment:",
              error, m_currentPipeline.id, program);

    GLint activeUniforms = 0;
    glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &activeUniforms);
    for (GLint i = 0; i < activeUniforms; ++i) {
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
        GLint unit = -1;
        if (location >= 0) {
            glGetUniformiv(program, location, &unit);
        }

        LOG_ERROR(LogCategory::Render, "    {} (type=0x{:x}) -> texture unit {}",
                  nameBuf, type, unit);
    }
}

void GfxCommandListWebGL::draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) {
    auto it = m_state->pipelines.find(m_currentPipeline.id);
    if (it == m_state->pipelines.end()) {
        return;
    }

    // Prevent feedback loop: unbind any textures that are attached to the current FBO
    m_state->unbindFramebufferTextures();

    GLenum topology = WebGLUtils::toWebGLTopology(it->second.topology);

    if (instanceCount > 1) {
        // WebGL 2.0 supports instanced rendering
        // Note: firstInstance is not supported in WebGL 2.0, must be 0
        if (firstInstance != 0) {

        }
        glDrawArraysInstanced(topology, firstVertex, vertexCount, instanceCount);
    } else {
        glDrawArrays(topology, firstVertex, vertexCount);
    }

    reportDrawError();
}

void GfxCommandListWebGL::drawIndexed(uint32_t indexCount, uint32_t instanceCount,
                                       uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) {
    auto it = m_state->pipelines.find(m_currentPipeline.id);
    if (it == m_state->pipelines.end()) {
        return;
    }

    // Prevent feedback loop: unbind any textures that are attached to the current FBO
    m_state->unbindFramebufferTextures();

    GLenum topology = WebGLUtils::toWebGLTopology(it->second.topology);
    GLenum indexType = (m_currentIndexFormat == IndexFormat::UInt16) ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
    size_t indexSize = (m_currentIndexFormat == IndexFormat::UInt16) ? sizeof(uint16_t) : sizeof(uint32_t);

    void* indices = reinterpret_cast<void*>(m_indexBufferOffset + firstIndex * indexSize);

    // Handle vertexOffset (baseVertex) - WebGL 2.0 doesn't have glDrawElementsBaseVertex
    // We provide a u_VertexOffset uniform that shaders can use for vertex ID adjustment
    // For meshes with pre-baked indices, this allows correct rendering
    if (vertexOffset != 0) {
        GLint vertexOffsetLoc = getUniformLocation("u_VertexOffset");
        if (vertexOffsetLoc != -1) {
            glUniform1i(vertexOffsetLoc, vertexOffset);
        } else {
            // Log once per frame to avoid spam (using static to track)
            static bool warnedBaseVertex = false;
            if (!warnedBaseVertex) {
                
                warnedBaseVertex = true;
            }
        }
    } else {
        // Reset vertex offset uniform if it exists
        GLint vertexOffsetLoc = getUniformLocation("u_VertexOffset");
        if (vertexOffsetLoc != -1) {
            glUniform1i(vertexOffsetLoc, 0);
        }
    }

    if (instanceCount > 1) {
        // WebGL 2.0 supports instanced rendering
        if (firstInstance != 0) {
            static bool warnedFirstInstance = false;
            if (!warnedFirstInstance) {
                
                warnedFirstInstance = true;
            }
        }

        glDrawElementsInstanced(topology, indexCount, indexType, indices, instanceCount);
    } else {
        glDrawElements(topology, indexCount, indexType, indices);
    }

    reportDrawError();
}

void GfxCommandListWebGL::barrier() {
    // WebGL 2.0 has limited barrier support
    // glMemoryBarrier equivalent is not available in WebGL 2.0
    // The closest we can do is glFinish or glFlush for synchronization
    glFlush();
}

void GfxCommandListWebGL::beginDebugMarker(const char* name) {
    // WebGL doesn't have GL debug markers like desktop OpenGL
    // However, we can use browser developer tools via console grouping
    // This helps with debugging in browser dev tools
#ifdef LUPINE_DEBUG
    if (m_state->supportsDebugOutput) {
        // Use Emscripten's EM_ASM to call console.group for browser dev tools
        EM_ASM({
            console.group(UTF8ToString($0));
        }, name);
    }
#else
    (void)name;
#endif
}

void GfxCommandListWebGL::endDebugMarker() {
    // Close the console group opened by beginDebugMarker
#ifdef LUPINE_DEBUG
    if (m_state->supportsDebugOutput) {
        EM_ASM({
            console.groupEnd();
        });
    }
#endif
}

void GfxCommandListWebGL::execute() {
    // WebGL executes commands immediately (no deferred execution)
    // This method exists for API compatibility with other backends
}

} // namespace lupine

#endif // __EMSCRIPTEN__
