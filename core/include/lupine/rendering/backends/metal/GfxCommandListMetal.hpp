#pragma once

#include "../../gfx/GfxCommandList.hpp"
#include "MetalState.hpp"
#include <vector>

namespace lupine {

#ifdef LUPINE_HAS_METAL
#ifdef __APPLE__

// Forward declaration
class GfxDeviceMetal;

/**
 * Metal implementation of command list.
 * Records commands using Metal's render command encoder.
 *
 * Metal uses a deferred rendering model with command buffers and command encoders.
 * Commands are recorded to encoders and submitted in batches.
 */
class GfxCommandListMetal : public IGfxCommandList {
public:
    GfxCommandListMetal(GfxDeviceMetal* device, MetalState* state);
    ~GfxCommandListMetal() override;

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

    // ===== Compute Shader Support =====

    /**
     * Bind a compute pipeline for dispatch operations.
     * @param pipelineId The compute pipeline ID from MetalState
     */
    void bindComputePipeline(uint32_t pipelineId);

    /**
     * Bind a buffer for compute shader access.
     * @param buffer The buffer handle
     * @param index The buffer binding index
     */
    void bindComputeBuffer(BufferHandle buffer, uint32_t index);

    /**
     * Bind a texture for compute shader access (read/write).
     * @param texture The texture handle
     * @param index The texture binding index
     */
    void bindComputeTexture(TextureHandle texture, uint32_t index);

    /**
     * Dispatch compute work.
     * @param groupCountX Number of thread groups in X dimension
     * @param groupCountY Number of thread groups in Y dimension
     * @param groupCountZ Number of thread groups in Z dimension
     */
    void dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);

    /**
     * Dispatch compute work with indirect arguments.
     * @param indirectBuffer Buffer containing dispatch arguments
     * @param offset Offset into the buffer
     */
    void dispatchIndirect(BufferHandle indirectBuffer, uint64_t offset);

    // Internal methods
    void beginRendering(MetalRenderTarget* target);
    void endRendering();

    // Get command buffer for submission
    id<MTLCommandBuffer> getCommandBuffer() const { return m_commandBuffer; }

private:
    GfxDeviceMetal* m_device;
    MetalState* m_state;

    // Command buffer and encoders
    id<MTLCommandBuffer> m_commandBuffer = nil;
    id<MTLRenderCommandEncoder> m_encoder = nil;
    id<MTLComputeCommandEncoder> m_computeEncoder = nil;

    // Current bindings
    RenderTargetHandle m_currentRenderTargetHandle;
    MetalRenderTarget* m_currentRenderTarget = nullptr;
    PipelineHandle m_currentPipeline;
    uint32_t m_currentComputePipelineId = 0;
    BufferHandle m_currentIndexBuffer;
    IndexFormat m_currentIndexFormat = IndexFormat::UInt32;
    uint64_t m_indexBufferOffset = 0;
    bool m_pipelineBoundToEncoder = false;  // Track if pipeline was set on current encoder

    // Clear values
    MTLClearColor m_clearColor = {0.2, 0.2, 0.2, 1.0};
    double m_clearDepth = 1.0;
    uint32_t m_clearStencil = 0;
    bool m_pendingColorClear = false;
    bool m_pendingDepthClear = false;
    bool m_pendingStencilClear = false;

    // Pending viewport and scissor (deferred until encoder is created)
    MTLViewport m_pendingViewport = {0, 0, 1, 1, 0, 1};
    MTLScissorRect m_pendingScissor = {0, 0, 1, 1};
    bool m_hasViewport = false;
    bool m_hasScissor = false;

    // Vertex buffer bindings (Metal supports multiple vertex buffers)
    struct VertexBufferBinding {
        BufferHandle buffer;
        uint64_t offset;
    };
    std::vector<VertexBufferBinding> m_vertexBuffers;

    // Material constant buffer (slot 0)
    // Holds all uniforms set via setUniform* methods
    static constexpr uint32_t MATERIAL_CB_SIZE = 64 * 1024;  // 64KB
    id<MTLBuffer> m_materialBuffer = nil;
    std::vector<uint8_t> m_materialData;
    bool m_materialDataDirty = false;

    // Bone data constant buffer (slot 2) for skeletal animation
    static constexpr uint32_t MAX_BONES = 128;
    static constexpr uint32_t BONE_CB_SIZE = MAX_BONES * sizeof(float) * 16;  // 128 * 64 bytes = 8KB
    id<MTLBuffer> m_boneBuffer = nil;
    std::vector<uint8_t> m_boneData;
    bool m_boneDataDirty = false;

    // Uniform location cache (maps uniform name to offset in material buffer)
    std::unordered_map<std::string, uint32_t> m_uniformLocationCache;
    uint32_t m_nextUniformOffset = 0;

    // Track if encoder is active
    bool m_encoderActive = false;
    bool m_computeEncoderActive = false;

    // Track if pushConstants was used (to avoid overwriting with material buffer)
    bool m_pushConstantsUsed = false;
    uint32_t m_pushConstantsSize = 0;

    // Store push constants data so setUniform* can update it after pushConstants is called
    // This is critical because applyPropertyOverrides sets uniforms AFTER pushConstants
    static constexpr uint32_t PUSH_CONSTANTS_MAX_SIZE = 512;
    std::vector<uint8_t> m_pushConstantsData;
    bool m_pushConstantsDirty = false;

    // Helper to get or allocate uniform location
    uint32_t getUniformLocation(const char* name, uint32_t size);

    // Helper to update material buffer before draw
    void updateMaterialBuffer();

    // Helper to update bone buffer before draw
    void updateBoneBuffer();

    // Helper to apply pending clears
    void applyPendingClears();

    // Helper to ensure encoder is ready
    void ensureEncoder();

    // Helper to end current encoder if active
    void endEncoder();

    // Helper to ensure compute encoder is ready
    void ensureComputeEncoder();

    // Helper to end current compute encoder if active
    void endComputeEncoder();
};

#endif // __APPLE__
#endif // LUPINE_HAS_METAL

} // namespace lupine
