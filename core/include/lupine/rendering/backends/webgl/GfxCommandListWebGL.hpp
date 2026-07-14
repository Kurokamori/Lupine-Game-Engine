#pragma once

#ifdef __EMSCRIPTEN__

#include "../../gfx/GfxCommandList.hpp"
#include "WebGLState.hpp"
#include <set>
#include <vector>
#include <unordered_map>

namespace lupine {

/**
 * WebGL implementation of command list.
 * Records commands and executes them when submitted.
 *
 * WebGL 2.0 is based on OpenGL ES 3.0, so this implementation is similar
 * to the OpenGL backend but adapted for WebGL's constraints:
 * - Single context (no multi-context VAO management)
 * - No geometry shaders
 * - Limited texture formats
 * - Canvas-based rendering
 */
class GfxCommandListWebGL : public IGfxCommandList {
public:
    GfxCommandListWebGL(WebGLState* state);
    ~GfxCommandListWebGL() override;

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
    WebGLState* m_state;

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
        uint64_t offset = 0;
    };
    std::vector<VertexBufferBinding> m_vertexBuffers;

    struct TextureBinding {
        TextureHandle texture;
        uint32_t unit = 0;
    };
    std::vector<TextureBinding> m_textures;

    struct UniformBufferBinding {
        UniformBufferHandle buffer;
        uint32_t binding = 0;
    };
    std::vector<UniformBufferBinding> m_uniformBuffers;

    // Helper to get uniform location with caching
    GLint getUniformLocation(const char* name);

    // Type-safe uniform setters that validate uniform type before setting
    // Returns true if the uniform was set successfully, false if type mismatch or not found
    bool setUniformSafe4f(GLint location, float x, float y, float z, float w);
    bool setUniformSafe3f(GLint location, float x, float y, float z);
    bool setUniformSafe1f(GLint location, float value);
    bool setUniformSafe1i(GLint location, int value);
    bool setUniformSafeMatrix4fv(GLint location, const float* value);

    // Cache for uniform types (populated on first query per program)
    // Maps uniform location to GL type (GL_FLOAT_VEC4, GL_FLOAT_VEC3, etc.)
    std::unordered_map<GLint, GLenum> m_uniformTypeCache;
    GLuint m_cachedProgram = 0;  // Program for which the cache is valid

    // Reports a GL error raised by a draw call, naming the pipeline/program and dumping its
    // sampler uniform -> texture unit assignment. The browser console only says WHAT went
    // wrong, never WHICH program, which makes errors like "Two textures of different types
    // use the same sampler location" very hard to attribute. Bounded so it cannot spam or
    // cost anything after startup.
    void reportDrawError();
    bool m_drawErrorChecksExhausted = false;
    int m_drawsChecked = 0;
    std::set<GLuint> m_reportedErrorPrograms;
};

} // namespace lupine

#endif // __EMSCRIPTEN__
