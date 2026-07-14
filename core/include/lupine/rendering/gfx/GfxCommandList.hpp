#pragma once

#include "../ResourceHandles.hpp"
#include "GfxTypes.hpp"
#include "lupine/math/Color.hpp"
#include "lupine/math/Vec2.hpp"
#include "lupine/math/Vec3.hpp"
#include "lupine/math/Vec4.hpp"
#include "lupine/math/Mat4.hpp"

namespace lupine {

// Import math types into lupine namespace for rendering
using math::Color;
using math::Vec2;
using math::Vec3;
using math::Vec4;
using math::Mat4;

/**
 * Command list for recording GPU commands.
 * Backend-agnostic interface for building command buffers.
 */
class IGfxCommandList {
public:
    virtual ~IGfxCommandList() = default;

    // Render target operations
    virtual void setRenderTarget(RenderTargetHandle target) = 0;
    virtual void setViewport(const Viewport& viewport) = 0;
    virtual void setScissor(const ScissorRect& scissor) = 0;

    // Clear operations
    virtual void clearColor(const Color& color) = 0;
    virtual void clearDepth(float depth) = 0;
    virtual void clearStencil(uint8_t stencil) = 0;

    // Pipeline state
    virtual void bindPipeline(PipelineHandle pipeline) = 0;
    virtual void setLineWidth(float width) = 0;

    // Resource binding
    virtual void bindVertexBuffer(BufferHandle buffer, uint32_t binding, uint64_t offset = 0) = 0;
    virtual void bindIndexBuffer(BufferHandle buffer, IndexFormat format, uint64_t offset = 0) = 0;
    virtual void bindUniformBuffer(UniformBufferHandle buffer, uint32_t binding, uint32_t set = 0) = 0;
    virtual void bindTexture(TextureHandle texture, uint32_t binding, uint32_t set = 0) = 0;
    virtual void bindSampler(SamplerHandle sampler, uint32_t binding, uint32_t set = 0) = 0;

    // Dynamic constants (push constants / inline uniforms)
    virtual void pushConstants(ShaderStage stage, const void* data, uint32_t size, uint32_t offset = 0) = 0;

    // Uniform setting (backend-agnostic)
    virtual void setUniformFloat(const char* name, float value) = 0;
    virtual void setUniformInt(const char* name, int value) = 0;
    virtual void setUniformBool(const char* name, bool value) = 0;
    virtual void setUniformVec2(const char* name, const Vec2& value) = 0;
    virtual void setUniformVec3(const char* name, const Vec3& value) = 0;
    virtual void setUniformVec4(const char* name, const Vec4& value) = 0;
    virtual void setUniformMat4(const char* name, const math::Mat4& value) = 0;
    virtual void setUniformMat4Array(const char* name, const math::Mat4* values, size_t count) = 0;
    virtual void setUniformColor(const char* name, const Color& value) = 0;

    // Reset material data to defaults (prevents data leakage between draw calls)
    virtual void resetMaterialData() = 0;

    // Draw commands
    virtual void draw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstVertex = 0, uint32_t firstInstance = 0) = 0;
    virtual void drawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t firstIndex = 0, int32_t vertexOffset = 0, uint32_t firstInstance = 0) = 0;

    // Synchronization / barriers
    virtual void barrier() = 0;

    // Debug markers (for GPU debugging tools)
    virtual void beginDebugMarker(const char* name) = 0;
    virtual void endDebugMarker() = 0;
};

} // namespace lupine
