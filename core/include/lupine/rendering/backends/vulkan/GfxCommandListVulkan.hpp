#pragma once

#include "../../gfx/GfxCommandList.hpp"
#include "VulkanState.hpp"
#include <vector>
#include <unordered_set>

namespace lupine {

#ifdef LUPINE_HAS_VULKAN

// Forward declaration
class GfxDeviceVulkan;

/**
 * Vulkan implementation of command list.
 * Records commands to a Vulkan command buffer.
 */
class GfxCommandListVulkan : public IGfxCommandList {
public:
    GfxCommandListVulkan(GfxDeviceVulkan* device, VulkanState* state, VkCommandBuffer commandBuffer);
    ~GfxCommandListVulkan() override;

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

    void draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) override;
    void drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) override;

    void barrier() override;

    void beginDebugMarker(const char* name) override;
    void endDebugMarker() override;

    // Internal - get the command buffer for submission
    VkCommandBuffer getCommandBuffer() const { return m_commandBuffer; }

    // Begin and end render pass
    void beginRenderPass(VulkanRenderTarget* target, const Color& clearColor, float clearDepth);
    void endRenderPass();

private:
    GfxDeviceVulkan* m_device;
    VulkanState* m_state;
    VkCommandBuffer m_commandBuffer;

    // Current bindings
    RenderTargetHandle m_currentRenderTargetHandle;
    VulkanRenderTarget* m_currentRenderTarget = nullptr;  // For dynamic rendering transitions
    PipelineHandle m_currentPipeline;
    BufferHandle m_currentIndexBuffer;
    IndexFormat m_currentIndexFormat = IndexFormat::UInt32;
    uint64_t m_indexBufferOffset = 0;
    bool m_indexBufferBound = false;  // Track if index buffer was successfully bound
    uint64_t m_boundIndexBufferSize = 0;  // Size of the bound index buffer for validation

    // Render pass state
    bool m_renderPassActive = false;

    // Clear values (set before beginRenderPass)
    // Default to dark gray similar to editor background
    Color m_clearColor{0.2f, 0.2f, 0.2f, 1.0f};
    float m_clearDepth = 1.0f;
    uint8_t m_clearStencil = 0;
    bool m_pendingColorClear = false;
    bool m_pendingDepthClear = false;
    bool m_pendingStencilClear = false;

    struct VertexBufferBinding {
        BufferHandle buffer;
        uint64_t offset;
    };
    std::vector<VertexBufferBinding> m_vertexBuffers;

    struct TextureBinding {
        TextureHandle texture;
        uint32_t binding;
    };
    std::vector<TextureBinding> m_textures;

    struct SamplerBinding {
        SamplerHandle sampler;
        uint32_t binding;
    };
    std::vector<SamplerBinding> m_samplers;

    struct UniformBufferBinding {
        UniformBufferHandle buffer;
        uint32_t binding;
    };
    std::vector<UniformBufferBinding> m_uniformBuffers;

    // Push constant data
    std::vector<uint8_t> m_pushConstantData;

    // Mat4 array staging buffer for bone matrices
    VkBuffer m_mat4ArrayStagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_mat4ArrayStagingMemory = VK_NULL_HANDLE;
    void* m_mat4ArrayMappedData = nullptr;
    size_t m_mat4ArrayBufferSize = 0;
    size_t m_mat4ArrayDataSize = 0;  // Actual size of bone data written this frame
    bool m_hasBoneData = false;  // Track if bone data was set this frame
    static constexpr size_t MAX_BONE_MATRICES = 256;

    // Per-draw material uniform block (set = 0, binding = 2).
    //
    // Every shader declares its OWN binding-2 layout: rounded_rect uses
    // {u_UseTexture, u_CornerRadius, u_Size, u_UVRect}, pbr uses
    // {u_View, u_CameraPosition, u_AlbedoColor, ...}, skybox uses the sky colors,
    // and so on. Rather than assume a single fixed struct (which never matched the
    // generated shaders), we keep a raw byte buffer and write each engine uniform
    // at the offset the bound pipeline reflected for it. Each draw snapshots the
    // current bytes into a ring-buffer slot so consecutive draws in a batch can
    // carry different per-item material data.
    static constexpr size_t MATERIAL_UBO_SIZE = 512;  // >= largest material block across all shaders
    std::vector<uint8_t> m_materialData;              // current material bytes (reflected layout)

    // Material UBO ring buffer - one slot per draw call within a frame.
    static constexpr size_t MAX_MATERIAL_UBOS_PER_FRAME = 4092;
    VkBuffer m_materialUBORingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_materialUBORingMemory = VK_NULL_HANDLE;
    void* m_materialUBORingMappedData = nullptr;
    uint32_t m_materialUBOIndex = 0;  // Current slot index in the ring buffer

    // Ensure material UBO is created
    void ensureMaterialUBO();

    // Reset material bytes to zero (call on pipeline bind so a new batch starts
    // clean). Material uniforms are then re-supplied per batch/item by the engine.
    // This is also the IGfxCommandList::resetMaterialData() override.
    void resetMaterialData() override;

    // Route an engine uniform (by name) into the per-draw material bytes if the
    // bound pipeline declares it in its binding-2 block. Copy is clamped to the
    // member's declared size. Returns true if it was a material-UBO member.
    bool writeMaterialUniform(const char* name, const void* data, uint32_t size);

    // Route an engine uniform (by name) into push constants if the bound pipeline
    // declares it in its push_constant block. Returns true if it was pushed.
    bool writePushConstantUniform(const char* name, const void* data, uint32_t size);

    // Update descriptor sets if bindings have changed
    void updateDescriptorSets();

    // Flush descriptor sets before drawing (update if dirty, then bind)
    void flushDescriptorSets();

    // Track if descriptor sets need to be updated before next draw
    bool m_descriptorSetsDirty = false;

    // Track descriptor sets that have been updated+bound this command buffer recording
    // Once a descriptor set is bound, we cannot update it again without invalidating the command buffer
    // Using uint64_t to avoid potential hash issues with VkDescriptorSet handle types
    std::unordered_set<uint64_t> m_boundDescriptorSetsThisFrame;

    // Ensure mat4 array staging buffer is created (fixed size, never resized)
    void ensureMat4ArrayBuffer();
};

#endif // LUPINE_HAS_VULKAN

} // namespace lupine
