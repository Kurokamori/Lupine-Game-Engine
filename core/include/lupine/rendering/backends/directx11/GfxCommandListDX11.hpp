#pragma once

#ifdef LUPINE_HAS_DIRECTX11

#include "../../gfx/GfxCommandList.hpp"
#include "DirectX11State.hpp"
#include <vector>

namespace lupine {

// Forward declaration
class GfxDeviceDX11;

/**
 * DirectX 11 implementation of command list.
 * Records commands for immediate execution on the D3D11 device context.
 *
 * Unlike Vulkan which records to command buffers, DX11 uses immediate-mode rendering
 * where commands are executed as they are issued. This class maintains state
 * and defers certain operations to match the command list abstraction.
 */
class GfxCommandListDX11 : public IGfxCommandList {
public:
    GfxCommandListDX11(GfxDeviceDX11* device, DirectX11State* state);
    ~GfxCommandListDX11() override;

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
    void beginRendering(DX11RenderTarget* target);
    void endRendering();

private:
    GfxDeviceDX11* m_device;
    DirectX11State* m_state;
    ID3D11DeviceContext* m_context;  // Cached context pointer

    // GPU debug-marker annotation interface (RenderDoc / VS Graphics Debugger / PIX).
    // Queried lazily from the device context; null when the runtime debug layer is absent.
    ComPtr<ID3DUserDefinedAnnotation> m_annotation;

    // Current bindings
    RenderTargetHandle m_currentRenderTargetHandle;
    DX11RenderTarget* m_currentRenderTarget = nullptr;
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

    // Vertex buffer bindings (DX11 supports multiple vertex buffers)
    struct VertexBufferBinding {
        BufferHandle buffer;
        uint64_t offset;
    };
    std::vector<VertexBufferBinding> m_vertexBuffers;

    // Constant buffer bindings (per-stage)
    static constexpr uint32_t MAX_CONSTANT_BUFFERS = 14;  // D3D11 supports 15, reserve slot 0 for material data
    ID3D11Buffer* m_vsConstantBuffers[MAX_CONSTANT_BUFFERS] = {};
    ID3D11Buffer* m_psConstantBuffers[MAX_CONSTANT_BUFFERS] = {};
    ID3D11Buffer* m_gsConstantBuffers[MAX_CONSTANT_BUFFERS] = {};
    ID3D11Buffer* m_hsConstantBuffers[MAX_CONSTANT_BUFFERS] = {};
    ID3D11Buffer* m_dsConstantBuffers[MAX_CONSTANT_BUFFERS] = {};

    // Texture/sampler bindings (per-stage)
    static constexpr uint32_t MAX_SHADER_RESOURCES = 16;
    ID3D11ShaderResourceView* m_vsTextures[MAX_SHADER_RESOURCES] = {};
    ID3D11ShaderResourceView* m_psTextures[MAX_SHADER_RESOURCES] = {};
    ID3D11SamplerState* m_vsSamplers[MAX_SHADER_RESOURCES] = {};
    ID3D11SamplerState* m_psSamplers[MAX_SHADER_RESOURCES] = {};

    // Material constant buffer (slot 0)
    // Holds all uniforms set via setUniform* methods
    static constexpr uint32_t MATERIAL_CB_SIZE = 64 * 1024;  // 64KB
    ComPtr<ID3D11Buffer> m_materialConstantBuffer;
    std::vector<uint8_t> m_materialData;
    bool m_materialDataDirty = false;

    // Bone data constant buffer (slot 2) for skeletal animation
    static constexpr uint32_t MAX_BONES = 128;
    static constexpr uint32_t BONE_CB_SIZE = MAX_BONES * sizeof(float) * 16;  // 128 * 64 bytes = 8KB
    ComPtr<ID3D11Buffer> m_boneConstantBuffer;
    std::vector<uint8_t> m_boneData;
    bool m_boneDataDirty = false;

    // Uniform location cache (maps uniform name to offset in material buffer)
    std::unordered_map<std::string, uint32_t> m_uniformLocationCache;
    uint32_t m_nextUniformOffset = 0;

    // Helper to get or allocate uniform location
    uint32_t getUniformLocation(const char* name, uint32_t size);

    // Helper to update material constant buffer
    void updateMaterialConstantBuffer();

    // Helper to update bone constant buffer
    void updateBoneConstantBuffer();

    // Helper to apply pending clears
    void applyPendingClears();
};

} // namespace lupine

#endif // LUPINE_HAS_DIRECTX11
