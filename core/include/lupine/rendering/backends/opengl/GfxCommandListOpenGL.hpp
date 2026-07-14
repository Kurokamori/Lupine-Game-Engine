#pragma once

#include "../../gfx/GfxCommandList.hpp"
#include "OpenGLState.hpp"
#include <vector>

namespace lupine {

/**
 * OpenGL implementation of command list.
 * Records commands and executes them when submitted.
 */
class GfxCommandListOpenGL : public IGfxCommandList {
public:
    GfxCommandListOpenGL(OpenGLState* state);
    ~GfxCommandListOpenGL() override;

    // IGfxCommandList interface
    void setRenderTarget(RenderTargetHandle target) override;
    void setViewport(const Viewport& viewport) override;
    void setScissor(const ScissorRect& scissor) override;

    void clearColor(const Color& color) override;
    void clearDepth(float depth) override;
    void clearStencil(uint8_t stencil) override;

    void bindPipeline(PipelineHandle pipeline) override;
    void setLineWidth(float width) override;

    void bindVertexBuffer(BufferHandle buffer, uint32_t binding, uint64_t offset) override;
    void bindIndexBuffer(BufferHandle buffer, IndexFormat format, uint64_t offset) override;
    void bindUniformBuffer(UniformBufferHandle buffer, uint32_t binding, uint32_t set) override;
    void bindTexture(TextureHandle texture, uint32_t binding, uint32_t set) override;
    void bindSampler(SamplerHandle sampler, uint32_t binding, uint32_t set) override;

    void pushConstants(ShaderStage stage, const void* data, uint32_t size, uint32_t offset) override;

    // Uniform setting (backend-agnostic interface implementation)
    void setUniformFloat(const char* name, float value) override;
    void setUniformInt(const char* name, int value) override;
    void setUniformBool(const char* name, bool value) override;
    void setUniformVec2(const char* name, const Vec2& value) override;
    void setUniformVec3(const char* name, const Vec3& value) override;
    void setUniformVec4(const char* name, const Vec4& value) override;
    void setUniformMat4(const char* name, const math::Mat4& value) override;
    void setUniformMat4Array(const char* name, const math::Mat4* values, size_t count) override;
    void setUniformColor(const char* name, const Color& value) override;

    void resetMaterialData() override;

    void draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) override;
    void drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) override;

    void barrier() override;

    void beginDebugMarker(const char* name) override;
    void endDebugMarker() override;

    // Internal - execute all recorded commands
    void execute();

private:
    OpenGLState* m_state;

    /// Uniform location lookup through the per-pipeline cache.
    /// glGetUniformLocation is a driver round-trip; setUniform* runs per draw.
    GLint getUniformLocationCached(GLPipeline& pipeline, const char* name);

    // Current bindings
    RenderTargetHandle m_currentRenderTarget;
    PipelineHandle m_currentPipeline;
    BufferHandle m_currentIndexBuffer;
    IndexFormat m_currentIndexFormat = IndexFormat::UInt32;
    uint64_t m_indexBufferOffset = 0;

    // Clear state tracking
    bool m_pendingColorClear = false;

    struct VertexBufferBinding {
        BufferHandle buffer;
        uint64_t offset;
    };
    std::vector<VertexBufferBinding> m_vertexBuffers;

    struct TextureBinding {
        TextureHandle texture;
        uint32_t unit;
    };
    std::vector<TextureBinding> m_textures;

    struct UniformBufferBinding {
        UniformBufferHandle buffer;
        uint32_t binding;
    };
    std::vector<UniformBufferBinding> m_uniformBuffers;
};

} // namespace lupine
