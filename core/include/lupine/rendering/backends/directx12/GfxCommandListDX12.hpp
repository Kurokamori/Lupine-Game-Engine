#pragma once

#ifdef LUPINE_HAS_DIRECTX12

#include "../../gfx/GfxCommandList.hpp"
#include "DirectX12State.hpp"
#include <vector>

namespace lupine {

// Forward declaration
class GfxDeviceDX12;

/**
 * DirectX 12 implementation of command list.
 * Records commands to a ID3D12GraphicsCommandList for deferred execution.
 *
 * Unlike DirectX 11's immediate mode, DX12 command lists are recorded and
 * then executed on the GPU. This allows for better CPU/GPU parallelism.
 */
class GfxCommandListDX12 : public IGfxCommandList {
public:
    GfxCommandListDX12(GfxDeviceDX12* device, DirectX12State* state);
    ~GfxCommandListDX12() override;

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

    // Internal methods
    void beginRendering(DX12RenderTarget* target);
    void endRendering();

    // Emit an EndEvent for every debug marker still open on this command list so a
    // marker never spans the Close() boundary. Called from the device's submit path
    // before the command list is closed and executed.
    void closeOpenDebugMarkers();

    // Get the underlying command list
    ID3D12GraphicsCommandList* getCommandList() const { return m_commandList; }

private:
    GfxDeviceDX12* m_device;
    DirectX12State* m_state;
    ID3D12GraphicsCommandList* m_commandList;

    // Current bindings
    RenderTargetHandle m_currentRenderTargetHandle;
    DX12RenderTarget* m_currentRenderTarget = nullptr;
    PipelineHandle m_currentPipeline;
    BufferHandle m_currentIndexBuffer;
    IndexFormat m_currentIndexFormat = IndexFormat::UInt32;
    uint64_t m_indexBufferOffset = 0;

    // Clear values (deferred until render target is set)
    Color m_clearColor{0.2f, 0.2f, 0.2f, 1.0f};
    float m_clearDepth = 1.0f;
    uint8_t m_clearStencil = 0;
    bool m_pendingColorClear = false;
    bool m_pendingDepthClear = false;
    bool m_pendingStencilClear = false;

    // Vertex buffer bindings
    struct VertexBufferBinding {
        BufferHandle buffer;
        uint64_t offset;
    };
    std::vector<VertexBufferBinding> m_vertexBuffers;
    bool m_vertexBuffersDirty = false;

    // Descriptor bindings
    static constexpr uint32_t MAX_BOUND_TEXTURES = 32;
    static constexpr uint32_t MAX_BOUND_SAMPLERS = 16;
    static constexpr uint32_t MAX_BOUND_CBVS = 14;

    TextureHandle m_boundTextures[MAX_BOUND_TEXTURES] = {};
    SamplerHandle m_boundSamplers[MAX_BOUND_SAMPLERS] = {};
    UniformBufferHandle m_boundCBVs[MAX_BOUND_CBVS] = {};
    bool m_descriptorsDirty = false;

    // SRV table tracking — only allocate a new descriptor table when textures
    // actually change, not on every draw.  CBV/sampler changes reuse the last table.
    bool m_srvTableDirty = false;
    D3D12_GPU_DESCRIPTOR_HANDLE m_lastSrvTableGpuHandle = {};  // reused when textures unchanged

    // Material constant buffer (slot 0)
    // Holds all uniforms set via setUniform* methods
    static constexpr uint32_t MATERIAL_CB_SIZE = 64 * 1024;  // 64KB
    std::vector<uint8_t> m_materialData;
    bool m_materialDataDirty = false;
    bool m_srvPoolExhausted = false;  // Set when SRV table pool is full — skip draw

    // Bone data constant buffer (slot 2) for skeletal animation
    static constexpr uint32_t MAX_BONES = 128;
    static constexpr uint32_t BONE_CB_SIZE = MAX_BONES * sizeof(float) * 16;  // 128 * 64 bytes = 8KB
    std::vector<uint8_t> m_boneData;
    bool m_boneDataDirty = false;

    // Uniform location cache (maps uniform name to offset in material buffer)
    std::unordered_map<std::string, uint32_t> m_uniformLocationCache;
    uint32_t m_nextUniformOffset = 0;

    // Rendering state
    bool m_isRendering = false;

    // Number of PIX debug markers currently open on this command list. BeginEvent is
    // conditional (it is skipped when the label can't be encoded), but the queue-level
    // validation requires EndEvent to never outnumber BeginEvent, so endDebugMarker only
    // emits an EndEvent while this depth is positive.
    int m_debugMarkerDepth = 0;

    // Helper to get or allocate uniform location
    uint32_t getUniformLocation(const char* name, uint32_t size);

    // Helper to update material constant buffer (returns false if heap exhausted)
    bool updateMaterialConstantBuffer();

    // Helper to update bone constant buffer (returns false if heap exhausted)
    bool updateBoneConstantBuffer();

    // Helper to apply pending clears
    void applyPendingClears();

    // Helper to apply vertex buffer bindings
    void applyVertexBuffers();

    // Resolve the vertex stride for a given input slot from the current
    // pipeline. Slot 0 uses the primary geometry layout; higher slots use the
    // matching per-binding entry in the pipeline's extra vertex buffer layouts.
    uint32_t resolveVertexStrideForBinding(uint32_t binding) const;

    // Helper to apply descriptor bindings
    void applyDescriptors();

    // Helper to transition render target resources
    void transitionRenderTarget(DX12RenderTarget* target, bool toRenderTarget);
};

} // namespace lupine

#endif // LUPINE_HAS_DIRECTX12
