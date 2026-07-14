#include "lupine/rendering/backends/vulkan/GfxCommandListVulkan.hpp"
#include "lupine/rendering/backends/vulkan/GfxDeviceVulkan.hpp"
#include "lupine/logger/Logger.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <cstring>
#include <array>
#include <algorithm>
#include <unordered_map>

#ifdef LUPINE_HAS_VULKAN

namespace lupine {

GfxCommandListVulkan::GfxCommandListVulkan(GfxDeviceVulkan* device, VulkanState* state, VkCommandBuffer commandBuffer)
    : m_device(device)
    , m_state(state)
    , m_commandBuffer(commandBuffer) {
    m_vertexBuffers.resize(8);
    m_pushConstantData.resize(256);  // Standard push constant size
    m_materialData.assign(MATERIAL_UBO_SIZE, 0);  // Per-draw material bytes (reflected layout)
}

GfxCommandListVulkan::~GfxCommandListVulkan() {
    // IMPORTANT: These buffers may still be referenced by the command buffer which hasn't
    // been executed yet. Use deferred destruction to ensure they outlive the GPU work.

    // Clean up mat4 array staging buffer using deferred destruction
    if (m_mat4ArrayStagingBuffer != VK_NULL_HANDLE) {
        // Unmap memory before deferring destruction
        if (m_mat4ArrayMappedData != nullptr) {
            vkUnmapMemory(m_state->device, m_mat4ArrayStagingMemory);
            m_mat4ArrayMappedData = nullptr;
        }
        // Defer destruction until GPU is done with this buffer
        VulkanBuffer deferredBuffer{};
        deferredBuffer.buffer = m_mat4ArrayStagingBuffer;
        deferredBuffer.memory = m_mat4ArrayStagingMemory;
        m_state->deferBufferDestroyNoMap(deferredBuffer);
        m_mat4ArrayStagingBuffer = VK_NULL_HANDLE;
        m_mat4ArrayStagingMemory = VK_NULL_HANDLE;
    }

    // Clean up material UBO ring buffer using deferred destruction
    if (m_materialUBORingBuffer != VK_NULL_HANDLE) {
        // Unmap memory before deferring destruction
        if (m_materialUBORingMappedData != nullptr) {
            vkUnmapMemory(m_state->device, m_materialUBORingMemory);
            m_materialUBORingMappedData = nullptr;
        }
        // Defer destruction until GPU is done with this buffer
        VulkanBuffer deferredBuffer{};
        deferredBuffer.buffer = m_materialUBORingBuffer;
        deferredBuffer.memory = m_materialUBORingMemory;
        m_state->deferBufferDestroyNoMap(deferredBuffer);
        m_materialUBORingBuffer = VK_NULL_HANDLE;
        m_materialUBORingMemory = VK_NULL_HANDLE;
    }
}

void GfxCommandListVulkan::setRenderTarget(RenderTargetHandle target) {
    // If we have an active render pass, end it first
    if (m_renderPassActive) {
        endRenderPass();
    }

    m_currentRenderTargetHandle = target;

    auto it = m_state->renderTargets.find(target.id);
    if (it != m_state->renderTargets.end()) {
        m_state->currentRenderTarget = &it->second;
        // Also set member variable so bindPipeline can get the correct formats
        m_currentRenderTarget = &it->second;
    } else {
        m_currentRenderTarget = nullptr;
    }
}

void GfxCommandListVulkan::setViewport(const Viewport& viewport) {
    // Vulkan has inverted Y compared to OpenGL
    VkViewport vkViewport{};
    vkViewport.x = viewport.x;
    // Flip Y for Vulkan (top-left origin)
    vkViewport.y = viewport.height - viewport.y;
    vkViewport.width = viewport.width;
    vkViewport.height = -viewport.height;  // Negative height for Y flip
    vkViewport.minDepth = viewport.minDepth;
    vkViewport.maxDepth = viewport.maxDepth;

    vkCmdSetViewport(m_commandBuffer, 0, 1, &vkViewport);
}

void GfxCommandListVulkan::setScissor(const ScissorRect& scissor) {
    VkRect2D vkScissor{};

    // If scissor has zero dimensions, it means "no scissor" - use a large rect
    if (scissor.width == 0 || scissor.height == 0) {
        // Use maximum possible scissor (effectively disabling scissor test)
        vkScissor.offset = {0, 0};
        vkScissor.extent = {16384, 16384};  // Large enough for any framebuffer
    } else {
        vkScissor.offset = {scissor.x, scissor.y};
        vkScissor.extent = {scissor.width, scissor.height};
    }

    vkCmdSetScissor(m_commandBuffer, 0, 1, &vkScissor);
}

void GfxCommandListVulkan::clearColor(const Color& color) {
    // Use color directly - we're now using linear (UNORM) framebuffers to match OpenGL
    m_clearColor.r = color.r;
    m_clearColor.g = color.g;
    m_clearColor.b = color.b;
    m_clearColor.a = color.a;
    m_pendingColorClear = true;
}

void GfxCommandListVulkan::clearDepth(float depth) {
    m_clearDepth = depth;
    m_pendingDepthClear = true;

    // In Vulkan, clears happen at render pass begin
    // If we have pending clears and a render target, begin render pass now
    if (m_currentRenderTargetHandle.isValid() && !m_renderPassActive) {
        auto it = m_state->renderTargets.find(m_currentRenderTargetHandle.id);
        if (it != m_state->renderTargets.end()) {
            beginRenderPass(&it->second, m_clearColor, m_clearDepth);
        }
    }
}

void GfxCommandListVulkan::clearStencil(uint8_t stencil) {
    m_clearStencil = stencil;
    m_pendingStencilClear = true;
}

void GfxCommandListVulkan::beginRenderPass(VulkanRenderTarget* target, const Color& clearColor, float clearDepth) {

    if (m_renderPassActive) {
        return;
    }

    if (target == nullptr) {
        
        return;
    }

    m_currentRenderTarget = target;

    // NOTE: Do NOT reset m_materialUBOIndex here - the ring buffer is per-frame, not per-render-pass.
    // Multiple render passes (shadow, main) within a single frame must use different indices.
    // The reset happens in beginFrame() instead.

    // Use VK_KHR_dynamic_rendering - no VkRenderPass needed
    // OPTIMIZATION: Batch layout transitions into a single vkCmdPipelineBarrier call
    std::array<VkImageMemoryBarrier, 2> barriers{};
    uint32_t barrierCount = 0;
    VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags dstStage = 0;

    // Transition color attachment to COLOR_ATTACHMENT_OPTIMAL
    if (target->hasColor && target->colorImage != VK_NULL_HANDLE) {
        auto& colorBarrier = barriers[barrierCount++];
        colorBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        colorBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        colorBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        colorBarrier.image = target->colorImage;
        colorBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        colorBarrier.subresourceRange.baseMipLevel = 0;
        colorBarrier.subresourceRange.levelCount = 1;
        colorBarrier.subresourceRange.baseArrayLayer = 0;
        colorBarrier.subresourceRange.layerCount = 1;
        colorBarrier.srcAccessMask = 0;
        colorBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dstStage |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    }

    // Transition depth attachment to DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    if (target->hasDepth && target->depthImage != VK_NULL_HANDLE) {
        auto& depthBarrier = barriers[barrierCount++];
        depthBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        depthBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        depthBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        depthBarrier.image = target->depthImage;
        depthBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (target->hasStencil) {
            depthBarrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
        depthBarrier.subresourceRange.baseMipLevel = 0;
        depthBarrier.subresourceRange.levelCount = 1;
        depthBarrier.subresourceRange.baseArrayLayer = 0;
        depthBarrier.subresourceRange.layerCount = 1;
        depthBarrier.srcAccessMask = 0;
        depthBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dstStage |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    }

    // Issue single batched barrier for all transitions
    if (barrierCount > 0) {
        vkCmdPipelineBarrier(m_commandBuffer, srcStage, dstStage,
            0, 0, nullptr, 0, nullptr, barrierCount, barriers.data());
    }

    // Setup color attachment info for dynamic rendering
    VkRenderingAttachmentInfo colorAttachmentInfo{};
    colorAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachmentInfo.imageView = target->colorView;
    colorAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachmentInfo.clearValue.color = {{clearColor.r, clearColor.g, clearColor.b, clearColor.a}};

    // Setup depth attachment info for dynamic rendering
    VkRenderingAttachmentInfo depthAttachmentInfo{};
    depthAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachmentInfo.imageView = target->depthView;
    depthAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachmentInfo.clearValue.depthStencil = {clearDepth, m_clearStencil};

    // Setup rendering info
    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea.offset = {0, 0};
    renderingInfo.renderArea.extent = {target->width, target->height};
    renderingInfo.layerCount = 1;
    renderingInfo.viewMask = 0;

    if (target->hasColor && target->colorView != VK_NULL_HANDLE) {
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments = &colorAttachmentInfo;
    } else if (target->hasColor) {
        
    }

    if (target->hasDepth && target->depthView != VK_NULL_HANDLE) {
        renderingInfo.pDepthAttachment = &depthAttachmentInfo;
        // NOTE: depthView is a depth-aspect-only view (so it can also be sampled),
        // so it cannot serve as a stencil attachment. The engine does not render
        // stencil, so no separate stencil attachment is bound.
    } else if (target->hasDepth) {

    }

    vkCmdBeginRendering(m_commandBuffer, &renderingInfo);

    // If this is a swapchain backbuffer, mark that a render pass was used
    // so present() knows not to add an extra layout transition barrier
    if (target->isSwapchainBackbuffer && target->owningSwapchainId != 0) {
        auto it = m_state->swapchains.find(target->owningSwapchainId);
        if (it != m_state->swapchains.end()) {
            VulkanSwapchain& swapchain = it->second;
            if (swapchain.currentFrame < swapchain.renderPassUsed.size()) {
                swapchain.renderPassUsed[swapchain.currentFrame] = true;
            }
        }
    } 
    m_renderPassActive = true;
    m_pendingColorClear = false;
    m_pendingDepthClear = false;
    m_pendingStencilClear = false;
}

void GfxCommandListVulkan::endRenderPass() {
    if (!m_renderPassActive) {
        return;
    }

    vkCmdEndRendering(m_commandBuffer);

    // Transition images to their final layouts after rendering
    if (m_currentRenderTarget) {
        VulkanRenderTarget* target = m_currentRenderTarget;

        // For swapchain targets, transition to PRESENT_SRC_KHR
        // For offscreen targets, transition to SHADER_READ_ONLY_OPTIMAL
        if (target->hasColor && target->colorImage != VK_NULL_HANDLE) {
            VkImageMemoryBarrier colorBarrier{};
            colorBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            colorBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colorBarrier.newLayout = target->isSwapchainBackbuffer ?
                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            colorBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            colorBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            colorBarrier.image = target->colorImage;
            colorBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            colorBarrier.subresourceRange.baseMipLevel = 0;
            colorBarrier.subresourceRange.levelCount = 1;
            colorBarrier.subresourceRange.baseArrayLayer = 0;
            colorBarrier.subresourceRange.layerCount = 1;
            colorBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            colorBarrier.dstAccessMask = target->isSwapchainBackbuffer ? 0 : VK_ACCESS_SHADER_READ_BIT;

            vkCmdPipelineBarrier(m_commandBuffer,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                target->isSwapchainBackbuffer ? VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0, 0, nullptr, 0, nullptr, 1, &colorBarrier);
        }

        // For shadow maps (depth-only), transition to SHADER_READ_ONLY
        if (target->hasDepth && !target->hasColor && target->depthImage != VK_NULL_HANDLE) {
            VkImageMemoryBarrier depthBarrier{};
            depthBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            depthBarrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            depthBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            depthBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            depthBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            depthBarrier.image = target->depthImage;
            depthBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            if (target->hasStencil) {
                depthBarrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
            }
            depthBarrier.subresourceRange.baseMipLevel = 0;
            depthBarrier.subresourceRange.levelCount = 1;
            depthBarrier.subresourceRange.baseArrayLayer = 0;
            depthBarrier.subresourceRange.layerCount = 1;
            depthBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            depthBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            vkCmdPipelineBarrier(m_commandBuffer,
                VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0, 0, nullptr, 0, nullptr, 1, &depthBarrier);
        }
    }

    m_renderPassActive = false;
    m_currentRenderTarget = nullptr;
}

void GfxCommandListVulkan::bindPipeline(PipelineHandle pipeline) {
    m_currentPipeline = pipeline;

    auto it = m_state->pipelines.find(pipeline.id);
    if (it == m_state->pipelines.end()) {
        
        return;
    }

    VulkanPipeline& vkPipeline = it->second;
    m_state->currentPipeline = &vkPipeline;

    // Determine the required formats from the current render target
    // Using UNORM (linear) to match OpenGL's color space
    VkFormat requiredColorFormat = VK_FORMAT_B8G8R8A8_UNORM;
    VkFormat requiredDepthFormat = VK_FORMAT_D24_UNORM_S8_UINT;

    if (m_currentRenderTarget) {
        // For depth-only render targets (e.g., shadow maps), use VK_FORMAT_UNDEFINED for color
        // This ensures we get a pipeline variant that matches the render pass (0 color attachments)
        requiredColorFormat = m_currentRenderTarget->hasColor ? m_currentRenderTarget->vkColorFormat : VK_FORMAT_UNDEFINED;
        requiredDepthFormat = m_currentRenderTarget->vkDepthFormat;

    } else {
        
    }

    // Check if we need a variant for the current render target format
    VkPipeline pipelineToUse = vkPipeline.pipeline;

    if (requiredColorFormat != vkPipeline.colorFormat || requiredDepthFormat != vkPipeline.depthFormat) {
        // Try to get existing variant
        VkPipeline variant = vkPipeline.getVariant(requiredColorFormat, requiredDepthFormat);

        if (variant == VK_NULL_HANDLE) {
            // Create new variant on-demand
            variant = m_device->createPipelineVariant(pipeline, requiredColorFormat, requiredDepthFormat);
        }

        if (variant != VK_NULL_HANDLE) {
            pipelineToUse = variant;
        } else {
            
        }
    }

    if (pipelineToUse == VK_NULL_HANDLE) {
        
        return;
    }

    vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineToUse);

    // Clear per-material resources (textures, samplers) so they don't contaminate
    // the new pipeline's descriptor set. Keep uniform buffers (light UBO) as those
    // persist across pipeline binds within a frame.
    m_textures.clear();
    m_samplers.clear();

    // A pipeline bind marks a new batch: start its material bytes clean so uniforms
    // from the previous batch (which used a different binding-2 layout) don't leak.
    ensureMaterialUBO();
    resetMaterialData();

    // Mark descriptor sets as dirty - they will be updated and bound before the next draw call
    // This defers descriptor set binding to avoid updating a bound descriptor set
    m_descriptorSetsDirty = true;
}

void GfxCommandListVulkan::setLineWidth(float width) {
    vkCmdSetLineWidth(m_commandBuffer, width);
}

void GfxCommandListVulkan::bindVertexBuffer(BufferHandle buffer, uint32_t binding, uint64_t offset) {
    if (binding >= m_vertexBuffers.size()) {
        
        return;
    }

    m_vertexBuffers[binding].buffer = buffer;
    m_vertexBuffers[binding].offset = offset;

    auto it = m_state->buffers.find(buffer.id);
    if (it != m_state->buffers.end()) {
        VkBuffer vkBuffer = it->second.buffer;
        VkDeviceSize vkOffset = offset;

        vkCmdBindVertexBuffers(m_commandBuffer, binding, 1, &vkBuffer, &vkOffset);
    } else {
        
    }
}

void GfxCommandListVulkan::bindIndexBuffer(BufferHandle buffer, IndexFormat format, uint64_t offset) {
    m_currentIndexBuffer = buffer;
    m_currentIndexFormat = format;
    m_indexBufferOffset = offset;

    auto it = m_state->buffers.find(buffer.id);
    if (it != m_state->buffers.end()) {
        VkIndexType indexType = VkUtils::toVkIndexType(format);

        vkCmdBindIndexBuffer(m_commandBuffer, it->second.buffer, offset, indexType);
        m_indexBufferBound = true;
        m_boundIndexBufferSize = it->second.size;
    } else {
        // Buffer not found - mark as not bound to prevent invalid draw calls
        m_indexBufferBound = false;
        m_boundIndexBufferSize = 0;
        
    }
}

void GfxCommandListVulkan::bindUniformBuffer(UniformBufferHandle buffer, uint32_t binding, uint32_t) {
    // Store for descriptor set update
    UniformBufferBinding ubBinding;
    ubBinding.buffer = buffer;
    ubBinding.binding = binding;

    // Check if we already have this binding
    bool found = false;
    for (auto& existing : m_uniformBuffers) {
        if (existing.binding == binding) {
            existing = ubBinding;
            found = true;
            break;
        }
    }
    if (!found) {
        m_uniformBuffers.push_back(ubBinding);
    }

    m_descriptorSetsDirty = true;
}

void GfxCommandListVulkan::bindTexture(TextureHandle texture, uint32_t binding, uint32_t) {
    // In Vulkan, the descriptor set layout has:
    // - Bindings 0-3: Uniform buffers
    // - Bindings 4-7: Textures (albedo, metallic/roughness, normal, emissive)
    // - Binding 8: Shadow map array[8]
    // - Binding 9: Shadow cube map array[8]
    // - Bindings 10-15: Additional textures
    //
    // The engine code can use either:
    // 1. OpenGL-style texture unit binding (0 = first texture) - for non-lit materials
    // 2. Direct Vulkan binding numbers (4-7) - for lit materials
    //
    // We need to handle both cases correctly.
    static constexpr uint32_t VULKAN_TEXTURE_START = 4;

    uint32_t vulkanBinding = binding;

    // Map OpenGL-style texture units to Vulkan bindings
    // Only remap bindings 0-3 (OpenGL texture units) to Vulkan bindings 4-7
    // Bindings 4+ are already in the correct range and should pass through as-is
    if (binding < 4) {
        // Texture units 0-3 -> Vulkan bindings 4-7
        vulkanBinding = binding + VULKAN_TEXTURE_START;
    }
    // All other bindings (4+) pass through as-is - they're already Vulkan binding numbers

    // Store for descriptor set update
    TextureBinding texBinding;
    texBinding.texture = texture;
    texBinding.binding = vulkanBinding;

    // Check if we already have this binding
    bool found = false;
    for (auto& existing : m_textures) {
        if (existing.binding == vulkanBinding) {
            existing = texBinding;
            found = true;
            break;
        }
    }
    if (!found) {
        m_textures.push_back(texBinding);
    }

    // NOTE: u_TextureFlags (which textures are active) is set explicitly by the
    // engine via setUniformVec4("u_TextureFlags", ...) and routed into the
    // material UBO at the offset the bound shader reflected for it.

    m_descriptorSetsDirty = true;
}

void GfxCommandListVulkan::bindSampler(SamplerHandle sampler, uint32_t binding, uint32_t) {
    // Apply the same binding offset as bindTexture() since samplers and textures are paired
    static constexpr uint32_t VULKAN_TEXTURE_START = 4;

    uint32_t vulkanBinding = binding;

    // Map OpenGL-style texture units to Vulkan bindings
    // Only remap bindings 0-3 (OpenGL texture units) to Vulkan bindings 4-7
    // Bindings 4+ are already in the correct range and should pass through as-is
    if (binding < 4) {
        vulkanBinding = binding + VULKAN_TEXTURE_START;
    }
    // All other bindings (4+) pass through as-is

    // Store for descriptor set update
    SamplerBinding sampBinding;
    sampBinding.sampler = sampler;
    sampBinding.binding = vulkanBinding;

    // Check if we already have this binding
    bool found = false;
    for (auto& existing : m_samplers) {
        if (existing.binding == vulkanBinding) {
            existing = sampBinding;
            found = true;
            break;
        }
    }
    if (!found) {
        m_samplers.push_back(sampBinding);
    }

    m_descriptorSetsDirty = true;
}

void GfxCommandListVulkan::updateDescriptorSets() {

    if (!m_state->currentPipeline) {
        return;
    }

    VulkanPipeline* pipeline = m_state->currentPipeline;

    if (pipeline->descriptorSets.empty()) {
        return;
    }

    // Get the current frame index
    uint32_t frameIndex = 0;
    if (m_state->currentSwapchain) {
        frameIndex = m_state->currentSwapchain->currentFrame;
    }

    // Get the next available descriptor set for this frame
    // This allows each draw call to have its own descriptor set with unique textures
    uint32_t descriptorSetIndex = pipeline->getNextDescriptorSetIndex(frameIndex);
    if (descriptorSetIndex >= pipeline->descriptorSets.size()) {

        return;
    }

    VkDescriptorSet descriptorSet = pipeline->descriptorSets[descriptorSetIndex];
    if (descriptorSet == VK_NULL_HANDLE) {
        return;
    }

    std::vector<VkWriteDescriptorSet> descriptorWrites;
    std::vector<VkDescriptorBufferInfo> bufferInfos;
    std::vector<VkDescriptorImageInfo> imageInfos;

    // Binding 0 is unused by every generated shader (per-frame/material data lives
    // in bindings 1-3). Bind the dummy uniform buffer so the descriptor slot is
    // always valid and never points at a destroyed buffer.
    VkDescriptorBufferInfo cameraBufferInfo{};
    cameraBufferInfo.buffer = m_state->dummyUniformBuffer;
    cameraBufferInfo.offset = 0;
    cameraBufferInfo.range = 256;

    VkWriteDescriptorSet cameraWrite{};
    cameraWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    cameraWrite.dstSet = descriptorSet;
    cameraWrite.dstBinding = 0;
    cameraWrite.dstArrayElement = 0;
    cameraWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    cameraWrite.descriptorCount = 1;
    cameraWrite.pBufferInfo = &cameraBufferInfo;
    descriptorWrites.push_back(cameraWrite);

    // Handle material UBO at binding 2
    // IMPORTANT: Always ensure material UBO exists because shaders expect it at binding 2
    ensureMaterialUBO();

    VkDescriptorBufferInfo materialBufferInfo{};

    // Snapshot the CURRENT material bytes (written at this shader's reflected
    // offsets) into a fresh ring-buffer slot and bind it. We do NOT reset the
    // bytes here: batch-level material uniforms (e.g. u_View, u_TextureFlags) are
    // set once per batch and must persist across every item's draw. resetMaterialData()
    // runs on the next pipeline bind instead.
    if (m_materialUBORingBuffer != VK_NULL_HANDLE && m_materialUBORingMappedData != nullptr) {
        size_t alignment = m_state->minUniformBufferOffsetAlignment;
        size_t alignedMaterialSize = ((MATERIAL_UBO_SIZE + alignment - 1) / alignment) * alignment;

        size_t offset = alignedMaterialSize * m_materialUBOIndex;
        uint8_t* dst = static_cast<uint8_t*>(m_materialUBORingMappedData) + offset;
        memcpy(dst, m_materialData.data(), MATERIAL_UBO_SIZE);

        materialBufferInfo.buffer = m_materialUBORingBuffer;
        materialBufferInfo.offset = offset;
        materialBufferInfo.range = MATERIAL_UBO_SIZE;

        // Advance to next slot in ring buffer (wrap around if needed)
        m_materialUBOIndex = (m_materialUBOIndex + 1) % MAX_MATERIAL_UBOS_PER_FRAME;
    } else {
        // Use dummy buffer to prevent stale bindings
        materialBufferInfo.buffer = m_state->dummyUniformBuffer;
        materialBufferInfo.offset = 0;
        materialBufferInfo.range = MATERIAL_UBO_SIZE;
    }

    VkWriteDescriptorSet materialWrite{};
    materialWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    materialWrite.dstSet = descriptorSet;
    materialWrite.dstBinding = 2;  // Material/Skybox data at binding 2
    materialWrite.dstArrayElement = 0;
    materialWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    materialWrite.descriptorCount = 1;
    materialWrite.pBufferInfo = &materialBufferInfo;
    descriptorWrites.push_back(materialWrite);

    // Descriptor set layout bindings:
    // 0-3: Uniform buffers
    // 4-7: Single textures (albedo, metallic/roughness, normal, emissive)
    // 8: Shadow map array[8]
    // 9: Shadow cube map array[8]
    // 10-15: Additional textures
    static constexpr uint32_t UBO_BINDING_START = 0;
    static constexpr uint32_t UBO_BINDING_END = 4;      // Exclusive
    static constexpr uint32_t TEXTURE_BINDING_START = 4;
    static constexpr uint32_t TEXTURE_BINDING_END = 8;  // Exclusive (before shadow maps)
    static constexpr uint32_t SHADOW_MAP_BINDING = 8;
    static constexpr uint32_t SHADOW_CUBE_MAP_BINDING = 9;
    static constexpr uint32_t ADDITIONAL_TEXTURE_START = 10;
    static constexpr uint32_t ADDITIONAL_TEXTURE_END = 16;
    static constexpr uint32_t MAX_SHADOW_MAPS = 8;

    std::array<VkDescriptorImageInfo, MAX_SHADOW_MAPS> shadowMapInfos{};
    std::array<VkDescriptorImageInfo, MAX_SHADOW_MAPS> shadowCubeMapInfos{};
    uint32_t shadowMapCount = 0;
    uint32_t shadowCubeMapCount = 0;

    // Reserve to prevent reallocation - need space for all possible texture bindings
    // (4-7 = 4 slots, 10-15 = 6 slots = 10 total texture bindings plus actual textures)
    // Also reserve extra for shadow maps (binding 8 and 9 with up to 8 entries each)
    static constexpr uint32_t MAX_TEXTURE_BINDINGS = 10;
    static constexpr uint32_t EXTRA_RESERVE = 32;  // Extra padding to ensure no reallocation
    bufferInfos.reserve(m_uniformBuffers.size() + 4);  // +4 for camera, material, bone, light UBOs
    imageInfos.reserve(m_textures.size() + MAX_TEXTURE_BINDINGS + MAX_SHADOW_MAPS * 2 + EXTRA_RESERVE);

    // Handle light UBO at binding 3 explicitly with a persistent local buffer info
    // This ensures the pointer stays valid until vkUpdateDescriptorSets is called
    static constexpr uint32_t LIGHT_UBO_BINDING = 3;
    VkDescriptorBufferInfo lightBufferInfo{};
    bool hasLightUBO = false;

    // First pass: find and handle the light UBO at binding 3
    bool foundLightBinding = false;
    for (const auto& ubBinding : m_uniformBuffers) {
        if (ubBinding.binding == LIGHT_UBO_BINDING) {
            foundLightBinding = true;
            // Validate buffer handle is still valid (important for scene transitions)
            if (!ubBinding.buffer.isValid()) {
                
                break;
            }
            auto it = m_state->uniformBuffers.find(ubBinding.buffer.id);
            if (it == m_state->uniformBuffers.end()) {
                
            } else if (it->second.buffer == VK_NULL_HANDLE) {
                
            } else if (it->second.size == 0) {
                
            } else {
                lightBufferInfo.buffer = it->second.buffer;
                lightBufferInfo.offset = 0;
                lightBufferInfo.range = it->second.size;
                hasLightUBO = true;
            }
            break;
        }
    }

    // ALWAYS write light UBO descriptor to binding 3 - either real or dummy
    // This prevents stale descriptor set bindings from pointing to destroyed buffers
    // when switching scenes
    VkDescriptorBufferInfo dummyLightBufferInfo{};
    if (!hasLightUBO) {
        // Use dedicated dummy light UBO (8KB, initialized to 0) to prevent stale bindings
        // and infinite shader loops during scene transitions. The regular dummy buffer (256 bytes)
        // is too small for LightUniformBuffer (~6.5KB) causing garbage lightCounts values.
        if (!foundLightBinding) {
            // No light binding requested - this is normal for non-lit materials
        } else {
            // Light binding was requested but buffer was invalid - this is the scene transition case
            
        }
        dummyLightBufferInfo.buffer = m_state->dummyLightUBO;
        dummyLightBufferInfo.offset = 0;
        dummyLightBufferInfo.range = VulkanState::DUMMY_LIGHT_UBO_SIZE;
    }

    VkWriteDescriptorSet lightWrite{};
    lightWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    lightWrite.dstSet = descriptorSet;
    lightWrite.dstBinding = LIGHT_UBO_BINDING;
    lightWrite.dstArrayElement = 0;
    lightWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    lightWrite.descriptorCount = 1;
    lightWrite.pBufferInfo = hasLightUBO ? &lightBufferInfo : &dummyLightBufferInfo;
    descriptorWrites.push_back(lightWrite);

    // Update other uniform buffer bindings (binding 1 for bones, skip 0, 2, 3 which are handled explicitly)
    // Handle binding 1 (bones UBO) - ALWAYS write something to prevent stale bindings
    static constexpr uint32_t BONES_UBO_BINDING = 1;
    VkDescriptorBufferInfo bonesBufferInfo{};
    bool hasBonesUBO = false;

    // First check if bone data was set via setUniformMat4Array (skeletal mesh rendering)
    if (m_hasBoneData && m_mat4ArrayStagingBuffer != VK_NULL_HANDLE && m_mat4ArrayDataSize > 0) {
        bonesBufferInfo.buffer = m_mat4ArrayStagingBuffer;
        bonesBufferInfo.offset = 0;
        // Always bind the full buffer size to ensure all bone indices can be safely accessed
        // The shader may access u_BoneTransforms[boneID] where boneID could be anywhere in the valid range
        // Using only m_mat4ArrayDataSize could cause out-of-bounds reads if boneID >= written bone count
        bonesBufferInfo.range = m_mat4ArrayBufferSize;
        hasBonesUBO = true;
        // Reset the bone data flag after using it (will be set again if next draw needs bones)
        m_hasBoneData = false;
    } else {
        // Fall back to looking for bones UBO in bound uniform buffers
        for (const auto& ubBinding : m_uniformBuffers) {
            if (ubBinding.binding == BONES_UBO_BINDING) {
                if (ubBinding.buffer.isValid()) {
                    auto it = m_state->uniformBuffers.find(ubBinding.buffer.id);
                    if (it != m_state->uniformBuffers.end() && it->second.buffer != VK_NULL_HANDLE && it->second.size > 0) {
                        bonesBufferInfo.buffer = it->second.buffer;
                        bonesBufferInfo.offset = 0;
                        bonesBufferInfo.range = it->second.size;
                        hasBonesUBO = true;
                    }
                }
                break;
            }
        }
    }

    // Use dummy buffer if no valid bones UBO found
    if (!hasBonesUBO) {
        bonesBufferInfo.buffer = m_state->dummyUniformBuffer;
        bonesBufferInfo.offset = 0;
        bonesBufferInfo.range = 256;
    }

    VkWriteDescriptorSet bonesWrite{};
    bonesWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    bonesWrite.dstSet = descriptorSet;
    bonesWrite.dstBinding = BONES_UBO_BINDING;
    bonesWrite.dstArrayElement = 0;
    bonesWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bonesWrite.descriptorCount = 1;
    bonesWrite.pBufferInfo = &bonesBufferInfo;
    descriptorWrites.push_back(bonesWrite);

    // Find default sampler from the state's samplers map
    VkSampler defaultSampler = VK_NULL_HANDLE;
    if (!m_samplers.empty()) {
        auto sampIt = m_state->samplers.find(m_samplers[0].sampler.id);
        if (sampIt != m_state->samplers.end()) {
            defaultSampler = sampIt->second.sampler;
        }
    }
    // If still no sampler, try to get any sampler from the state
    if (defaultSampler == VK_NULL_HANDLE && !m_state->samplers.empty()) {
        defaultSampler = m_state->samplers.begin()->second.sampler;
    }
    // Ultimate fallback: use the dummy sampler from VulkanState
    if (defaultSampler == VK_NULL_HANDLE) {
        defaultSampler = m_state->dummySampler;
    }

    // Update texture bindings (combined image samplers)
    // Debug: log texture count
    if (!m_textures.empty()) {
        
    }

    // Build a map of bound textures for quick lookup
    std::unordered_map<uint32_t, size_t> boundTexturesByBinding;  // binding -> index in m_textures
    for (size_t i = 0; i < m_textures.size(); i++) {
        boundTexturesByBinding[m_textures[i].binding] = i;
    }

    // IMPORTANT: Always write ALL PBR texture bindings (4-7) with either real textures or dummy.
    // This prevents stale textures from previous draw calls leaking through when
    // descriptor sets are reused across frames.
    VkSampler dummySamplerHandle = m_state->dummySampler;
    VkImageView dummyViewHandle = m_state->dummyTextureView;

    // Helper to write a texture binding (real or dummy)
    auto writeTextureBinding = [&](uint32_t binding) {
        auto boundIt = boundTexturesByBinding.find(binding);

        if (boundIt != boundTexturesByBinding.end()) {
            // Real texture is bound to this slot
            const auto& texBinding = m_textures[boundIt->second];
            auto texIt = m_state->textures.find(texBinding.texture.id);

            if (texIt != m_state->textures.end() && texIt->second.view != VK_NULL_HANDLE) {
                // Find sampler for this binding
                VkSampler sampler = defaultSampler;
                for (const auto& sampBinding : m_samplers) {
                    if (sampBinding.binding == binding) {
                        auto sampIt = m_state->samplers.find(sampBinding.sampler.id);
                        if (sampIt != m_state->samplers.end()) {
                            sampler = sampIt->second.sampler;
                        }
                        break;
                    }
                }
                if (sampler == VK_NULL_HANDLE) {
                    sampler = dummySamplerHandle;
                }
                if (sampler == VK_NULL_HANDLE) {
                    return;  // Can't write without a sampler
                }

                VkDescriptorImageInfo imageInfo{};
                imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                imageInfo.imageView = texIt->second.view;
                imageInfo.sampler = sampler;
                imageInfos.push_back(imageInfo);

                VkWriteDescriptorSet descriptorWrite{};
                descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                descriptorWrite.dstSet = descriptorSet;
                descriptorWrite.dstBinding = binding;
                descriptorWrite.dstArrayElement = 0;
                descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                descriptorWrite.descriptorCount = 1;
                descriptorWrite.pImageInfo = &imageInfos.back();
                descriptorWrites.push_back(descriptorWrite);
                return;
            }
        }

        // No valid texture bound - write dummy texture
        if (dummyViewHandle != VK_NULL_HANDLE && dummySamplerHandle != VK_NULL_HANDLE) {
            VkDescriptorImageInfo dummyImageInfo{};
            dummyImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            dummyImageInfo.imageView = dummyViewHandle;
            dummyImageInfo.sampler = dummySamplerHandle;
            imageInfos.push_back(dummyImageInfo);

            VkWriteDescriptorSet dummyWrite{};
            dummyWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            dummyWrite.dstSet = descriptorSet;
            dummyWrite.dstBinding = binding;
            dummyWrite.dstArrayElement = 0;
            dummyWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            dummyWrite.descriptorCount = 1;
            dummyWrite.pImageInfo = &imageInfos.back();
            descriptorWrites.push_back(dummyWrite);
        }
    };

    // Write all PBR texture bindings (4-7)
    for (uint32_t binding = TEXTURE_BINDING_START; binding < TEXTURE_BINDING_END; ++binding) {
        writeTextureBinding(binding);
    }

    // Process remaining textures (shadow maps and additional textures)
    for (size_t i = 0; i < m_textures.size(); i++) {
        const auto& texBinding = m_textures[i];

        // Skip PBR textures (already handled above)
        if (texBinding.binding >= TEXTURE_BINDING_START && texBinding.binding < TEXTURE_BINDING_END) {
            continue;
        }

        // Skip bindings that are in the UBO range (0-3)
        if (texBinding.binding < UBO_BINDING_END) {
            
            continue;
        }

        auto texIt = m_state->textures.find(texBinding.texture.id);
        if (texIt == m_state->textures.end()) {
            
            continue;
        }

        // Validate the texture view is valid
        if (texIt->second.view == VK_NULL_HANDLE) {
            
            continue;
        }

        // Find matching sampler for this binding
        VkSampler sampler = VK_NULL_HANDLE;
        for (const auto& sampBinding : m_samplers) {
            if (sampBinding.binding == texBinding.binding) {
                auto sampIt = m_state->samplers.find(sampBinding.sampler.id);
                if (sampIt != m_state->samplers.end()) {
                    sampler = sampIt->second.sampler;
                }
                break;
            }
        }
        if (sampler == VK_NULL_HANDLE) {
            sampler = defaultSampler;
        }

        // Skip if we still have no valid sampler
        if (sampler == VK_NULL_HANDLE) {
            
            continue;
        }

        // Check for skybox texture bindings (24-25) - these are used for cubemap and panoramic skyboxes
        static constexpr uint32_t SKYBOX_TEXTURE_START = 24;
        static constexpr uint32_t SKYBOX_TEXTURE_END = 26;
        if (texBinding.binding >= SKYBOX_TEXTURE_START && texBinding.binding < SKYBOX_TEXTURE_END) {
            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = texIt->second.view;
            imageInfo.sampler = sampler;
            imageInfos.push_back(imageInfo);

            VkWriteDescriptorSet descriptorWrite{};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = descriptorSet;
            descriptorWrite.dstBinding = texBinding.binding;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pImageInfo = &imageInfos.back();
            descriptorWrites.push_back(descriptorWrite);
            continue;
        }

        // Check for additional texture bindings (10-15)
        if (texBinding.binding >= ADDITIONAL_TEXTURE_START && texBinding.binding < ADDITIONAL_TEXTURE_END) {
            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = texIt->second.view;
            imageInfo.sampler = sampler;
            imageInfos.push_back(imageInfo);

            VkWriteDescriptorSet descriptorWrite{};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = descriptorSet;
            descriptorWrite.dstBinding = texBinding.binding;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pImageInfo = &imageInfos.back();
            descriptorWrites.push_back(descriptorWrite);
            continue;
        }

        // Check if this is a shadow map binding (bindings 8-15 go to array at binding 8)
        if (texBinding.binding >= SHADOW_MAP_BINDING &&
            texBinding.binding < SHADOW_MAP_BINDING + MAX_SHADOW_MAPS) {
            // This is a 2D shadow map - collect for array binding
            uint32_t arrayIndex = texBinding.binding - SHADOW_MAP_BINDING;
            if (arrayIndex < MAX_SHADOW_MAPS) {
                VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                if (VkUtils::isDepthFormat(texIt->second.format)) {
                    layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
                }
                shadowMapInfos[arrayIndex].imageLayout = layout;
                shadowMapInfos[arrayIndex].imageView = texIt->second.view;
                shadowMapInfos[arrayIndex].sampler = sampler;
                shadowMapCount = std::max(shadowMapCount, arrayIndex + 1);
            }
            continue;  // Don't add individual write, we'll batch them
        }
        // Check if this is a shadow cube map binding (bindings 16-23 go to array at binding 9)
        else if (texBinding.binding >= SHADOW_MAP_BINDING + MAX_SHADOW_MAPS &&
                 texBinding.binding < SHADOW_MAP_BINDING + 2 * MAX_SHADOW_MAPS) {
            uint32_t arrayIndex = texBinding.binding - (SHADOW_MAP_BINDING + MAX_SHADOW_MAPS);
            if (arrayIndex < MAX_SHADOW_MAPS) {
                VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                if (VkUtils::isDepthFormat(texIt->second.format)) {
                    layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
                }
                shadowCubeMapInfos[arrayIndex].imageLayout = layout;
                shadowCubeMapInfos[arrayIndex].imageView = texIt->second.view;
                shadowCubeMapInfos[arrayIndex].sampler = sampler;
                shadowCubeMapCount = std::max(shadowCubeMapCount, arrayIndex + 1);
            }
            continue;  // Don't add individual write, we'll batch them
        }

    }

    // ALWAYS write additional texture bindings (10-15) with dummy values if not bound
    // This prevents stale bindings during scene transitions
    std::unordered_set<uint32_t> writtenAdditionalBindings;
    for (const auto& write : descriptorWrites) {
        if (write.dstBinding >= ADDITIONAL_TEXTURE_START && write.dstBinding < ADDITIONAL_TEXTURE_END) {
            writtenAdditionalBindings.insert(write.dstBinding);
        }
    }
    for (uint32_t binding = ADDITIONAL_TEXTURE_START; binding < ADDITIONAL_TEXTURE_END; ++binding) {
        if (writtenAdditionalBindings.find(binding) == writtenAdditionalBindings.end()) {
            // Not written yet - write dummy texture
            if (dummySamplerHandle != VK_NULL_HANDLE && dummyViewHandle != VK_NULL_HANDLE) {
                VkDescriptorImageInfo dummyInfo{};
                dummyInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                dummyInfo.imageView = dummyViewHandle;
                dummyInfo.sampler = dummySamplerHandle;
                imageInfos.push_back(dummyInfo);

                VkWriteDescriptorSet dummyWrite{};
                dummyWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                dummyWrite.dstSet = descriptorSet;
                dummyWrite.dstBinding = binding;
                dummyWrite.dstArrayElement = 0;
                dummyWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                dummyWrite.descriptorCount = 1;
                dummyWrite.pImageInfo = &imageInfos.back();
                descriptorWrites.push_back(dummyWrite);
            }
        }
    }

    // ALWAYS write skybox texture bindings (24-25) with dummy values if not bound
    static constexpr uint32_t SKYBOX_BINDING_START = 24;
    static constexpr uint32_t SKYBOX_BINDING_END = 26;
    std::unordered_set<uint32_t> writtenSkyboxBindings;
    for (const auto& write : descriptorWrites) {
        if (write.dstBinding >= SKYBOX_BINDING_START && write.dstBinding < SKYBOX_BINDING_END) {
            writtenSkyboxBindings.insert(write.dstBinding);
        }
    }
    for (uint32_t binding = SKYBOX_BINDING_START; binding < SKYBOX_BINDING_END; ++binding) {
        if (writtenSkyboxBindings.find(binding) == writtenSkyboxBindings.end()) {
            // Not written yet - write dummy texture
            // Use dummy cube map for binding 24 (cubemap), dummy 2D for binding 25 (panoramic)
            VkImageView dummyView = (binding == 24) ? m_state->dummyCubeMapView : dummyViewHandle;
            if (dummySamplerHandle != VK_NULL_HANDLE && dummyView != VK_NULL_HANDLE) {
                VkDescriptorImageInfo dummyInfo{};
                dummyInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                dummyInfo.imageView = dummyView;
                dummyInfo.sampler = dummySamplerHandle;
                imageInfos.push_back(dummyInfo);

                VkWriteDescriptorSet dummyWrite{};
                dummyWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                dummyWrite.dstSet = descriptorSet;
                dummyWrite.dstBinding = binding;
                dummyWrite.dstArrayElement = 0;
                dummyWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                dummyWrite.descriptorCount = 1;
                dummyWrite.pImageInfo = &imageInfos.back();
                descriptorWrites.push_back(dummyWrite);
            }
        }
    }

    // ALWAYS write shadow map arrays to prevent stale bindings during scene transitions
    // Fill any unbound slots with dummy textures
    VkDescriptorImageInfo dummyShadowMapInfo{};
    dummyShadowMapInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    dummyShadowMapInfo.imageView = m_state->dummyTextureView;
    dummyShadowMapInfo.sampler = m_state->dummySampler;

    VkDescriptorImageInfo dummyShadowCubeInfo{};
    dummyShadowCubeInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    dummyShadowCubeInfo.imageView = m_state->dummyCubeMapView;
    dummyShadowCubeInfo.sampler = m_state->dummySampler;

    // Fill remaining shadow map slots with dummy textures
    for (uint32_t i = shadowMapCount; i < MAX_SHADOW_MAPS; ++i) {
        shadowMapInfos[i] = dummyShadowMapInfo;
    }
    // Ensure all slots have valid samplers (fill missing ones with dummy)
    for (uint32_t i = 0; i < MAX_SHADOW_MAPS; ++i) {
        if (shadowMapInfos[i].sampler == VK_NULL_HANDLE) {
            shadowMapInfos[i] = dummyShadowMapInfo;
        }
    }

    // Always write all 8 shadow map entries
    VkWriteDescriptorSet shadowMapWrite{};
    shadowMapWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    shadowMapWrite.dstSet = descriptorSet;
    shadowMapWrite.dstBinding = SHADOW_MAP_BINDING;
    shadowMapWrite.dstArrayElement = 0;
    shadowMapWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    shadowMapWrite.descriptorCount = MAX_SHADOW_MAPS;
    shadowMapWrite.pImageInfo = shadowMapInfos.data();
    descriptorWrites.push_back(shadowMapWrite);

    // Fill remaining shadow cube map slots with dummy textures
    for (uint32_t i = shadowCubeMapCount; i < MAX_SHADOW_MAPS; ++i) {
        shadowCubeMapInfos[i] = dummyShadowCubeInfo;
    }
    // Ensure all slots have valid samplers
    for (uint32_t i = 0; i < MAX_SHADOW_MAPS; ++i) {
        if (shadowCubeMapInfos[i].sampler == VK_NULL_HANDLE) {
            shadowCubeMapInfos[i] = dummyShadowCubeInfo;
        }
    }

    // Always write all 8 shadow cube map entries
    VkWriteDescriptorSet shadowCubeMapWrite{};
    shadowCubeMapWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    shadowCubeMapWrite.dstSet = descriptorSet;
    shadowCubeMapWrite.dstBinding = SHADOW_CUBE_MAP_BINDING;
    shadowCubeMapWrite.dstArrayElement = 0;
    shadowCubeMapWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    shadowCubeMapWrite.descriptorCount = MAX_SHADOW_MAPS;
    shadowCubeMapWrite.pImageInfo = shadowCubeMapInfos.data();
    descriptorWrites.push_back(shadowCubeMapWrite);

    // OPTIMIZATION: Removed per-draw validation loop that was copying to validWrites vector
    // The dummy resources ensure all bindings are valid by construction
    // Only validate in debug builds if needed
    if (!descriptorWrites.empty()) {
        vkUpdateDescriptorSets(m_state->device,
                               static_cast<uint32_t>(descriptorWrites.size()),
                               descriptorWrites.data(), 0, nullptr);
    }

    // Always bind the descriptor set (it's pre-initialized with dummy values)
    vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline->layout, 0, 1, &descriptorSet, 0, nullptr);
}

void GfxCommandListVulkan::flushDescriptorSets() {
    // Only update and bind if marked dirty
    if (!m_descriptorSetsDirty) {
        return;
    }
    m_descriptorSetsDirty = false;
    updateDescriptorSets();
}

bool GfxCommandListVulkan::writePushConstantUniform(const char* name, const void* data, uint32_t size) {
    if (!m_state->currentPipeline) return false;
    auto it = m_state->currentPipeline->pushConstantOffsets.find(name);
    if (it == m_state->currentPipeline->pushConstantOffsets.end()) return false;

    // The pipeline push-constant range is 256 bytes; clamp to stay in range.
    static constexpr uint32_t MAX_PUSH_CONSTANT_SIZE = 256;
    uint32_t offset = it->second;
    if (offset >= MAX_PUSH_CONSTANT_SIZE) return true;  // declared but out of range; treat as handled
    uint32_t effectiveSize = size;
    if (offset + effectiveSize > MAX_PUSH_CONSTANT_SIZE) {
        effectiveSize = MAX_PUSH_CONSTANT_SIZE - offset;
    }
    if (effectiveSize == 0) return true;

    if (offset + effectiveSize <= m_pushConstantData.size()) {
        memcpy(m_pushConstantData.data() + offset, data, effectiveSize);
    }

    // Push to both stages: the pipeline layout's push-constant range spans
    // VERTEX|FRAGMENT, and the spec requires stageFlags to cover the whole range.
    vkCmdPushConstants(m_commandBuffer, m_state->currentPipeline->layout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       offset, effectiveSize, data);
    return true;
}

bool GfxCommandListVulkan::writeMaterialUniform(const char* name, const void* data, uint32_t size) {
    if (!m_state->currentPipeline) return false;
    auto it = m_state->currentPipeline->materialUBOOffsets.find(name);
    if (it == m_state->currentPipeline->materialUBOOffsets.end()) return false;

    ensureMaterialUBO();
    uint32_t offset = it->second.offset;
    uint32_t memberSize = it->second.size;
    uint32_t copySize = (size < memberSize) ? size : memberSize;
    if (copySize > 0 && offset + copySize <= m_materialData.size()) {
        memcpy(m_materialData.data() + offset, data, copySize);
        m_descriptorSetsDirty = true;
    }
    return true;
}

void GfxCommandListVulkan::pushConstants(ShaderStage stage, const void* data, uint32_t size, uint32_t offset) {
    (void)stage;
    if (!m_state->currentPipeline || data == nullptr || size == 0) {
        return;
    }

    // The engine pushes one canonical 352-byte PerObjectUniforms blob per draw
    // (the same layout DX11/DX12 use as a single constant buffer). Vulkan splits
    // that data across a <=256-byte push-constant block plus the binding-2 material
    // UBO, and each shader lays those out differently. Decompose the blob field by
    // field and route each one to wherever the bound shader actually declares it.
    //
    // Some slots are overloaded by the engine (e.g. offset 240 carries either
    // u_TextureFlags or u_CornerRadius depending on the shader); we attempt every
    // alias and only the name the bound shader declares is written.
    struct CanonicalField { const char* name; uint32_t offset; uint32_t size; };
    static const CanonicalField kCanonicalFields[] = {
        {"u_ViewProjection",   0,   64},
        {"u_Model",            64,  64},
        {"u_NormalMatrix",     128, 64},
        {"u_TintColor",        192, 16},
        {"u_UseTexture",       208, 4},
        {"u_AlphaCutoff",      212, 4},
        {"u_UVRect",           224, 16},
        {"u_TextureFlags",     240, 16},
        {"u_CornerRadius",     240, 16},
        {"u_GradientParams",   240, 16},
        {"u_PolygonParams",    240, 16},
        {"u_MaterialParams1",  256, 16},
        {"u_Size",             256, 8},
        {"u_MaterialParams2",  272, 16},
        {"u_CameraPosition",   288, 16},
        {"u_AlbedoColor",      304, 16},
        {"u_EmissiveColor",    320, 16},
        {"u_ReceiveShadow",    336, 4},
    };

    const uint8_t* blob = static_cast<const uint8_t*>(data) + offset;
    uint32_t available = size;
    for (const CanonicalField& field : kCanonicalFields) {
        if (field.offset + field.size > available) continue;
        const void* src = blob + field.offset;
        // A name lives in at most one block per shader; attempt both so the
        // matching one (push constant OR material UBO) gets written.
        writePushConstantUniform(field.name, src, field.size);
        writeMaterialUniform(field.name, src, field.size);
    }
}

void GfxCommandListVulkan::setUniformFloat(const char* name, float value) {
    writeMaterialUniform(name, &value, sizeof(float));
    writePushConstantUniform(name, &value, sizeof(float));
}

void GfxCommandListVulkan::setUniformInt(const char* name, int value) {
    writeMaterialUniform(name, &value, sizeof(int));
    writePushConstantUniform(name, &value, sizeof(int));
}

void GfxCommandListVulkan::setUniformBool(const char* name, bool value) {
    setUniformInt(name, value ? 1 : 0);
}

void GfxCommandListVulkan::setUniformVec2(const char* name, const Vec2& value) {
    float data[2] = {value.x, value.y};
    writeMaterialUniform(name, data, sizeof(data));
    writePushConstantUniform(name, data, sizeof(data));
}

void GfxCommandListVulkan::setUniformVec3(const char* name, const Vec3& value) {
    float data[3] = {value.x, value.y, value.z};
    writeMaterialUniform(name, data, sizeof(data));
    writePushConstantUniform(name, data, sizeof(data));
}

void GfxCommandListVulkan::setUniformVec4(const char* name, const Vec4& value) {
    float data[4] = {value.x, value.y, value.z, value.w};
    writeMaterialUniform(name, data, sizeof(data));
    writePushConstantUniform(name, data, sizeof(data));
}

void GfxCommandListVulkan::setUniformMat4(const char* name, const math::Mat4& value) {
    const float* data = glm::value_ptr(value.GetGLM());
    writeMaterialUniform(name, data, sizeof(float) * 16);
    writePushConstantUniform(name, data, sizeof(float) * 16);
}

void GfxCommandListVulkan::setUniformMat4Array(const char*, const math::Mat4* values, size_t count) {
    // Matrix arrays (like bone transforms) are stored in uniform buffers in Vulkan
    // Binding 1 is reserved for bone data in the descriptor set layout
    // We just store the data here and let updateDescriptorSets() handle the binding
    // along with all other bindings (camera, material, textures) to avoid partial
    // descriptor set updates that cause rendering corruption.
    if (count == 0 || values == nullptr) return;

    // Clamp count to maximum supported bones
    if (count > MAX_BONE_MATRICES) {
        count = MAX_BONE_MATRICES;
    }

    // Calculate required size
    size_t dataSize = count * sizeof(float) * 16;  // Each Mat4 is 16 floats

    // Ensure we have a staging buffer (pre-allocated at fixed size)
    ensureMat4ArrayBuffer();

    if (m_mat4ArrayMappedData == nullptr) {
        return;
    }

    // Copy matrix data to the staging buffer
    float* dst = static_cast<float*>(m_mat4ArrayMappedData);
    for (size_t i = 0; i < count; ++i) {
        const float* src = glm::value_ptr(values[i].GetGLM());
        memcpy(dst + i * 16, src, sizeof(float) * 16);
    }

    // Store the data size and mark that we have bone data
    m_mat4ArrayDataSize = dataSize;
    m_hasBoneData = true;

    // Mark descriptor sets as dirty so updateDescriptorSets() will be called
    // with the bone data properly integrated with all other bindings
    m_descriptorSetsDirty = true;
}

void GfxCommandListVulkan::ensureMat4ArrayBuffer() {
    // Pre-allocate buffer at fixed maximum size to avoid resizing during rendering
    // This prevents synchronization issues where the buffer is destroyed while in use
    if (m_mat4ArrayStagingBuffer != VK_NULL_HANDLE) {
        return;  // Buffer already exists
    }

    // Fixed size for maximum bone matrices (256 bones * 64 bytes per mat4 = 16KB)
    size_t bufferSize = MAX_BONE_MATRICES * sizeof(float) * 16;

    // Create the buffer
    m_state->createBuffer(
        bufferSize,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        m_mat4ArrayStagingBuffer,
        m_mat4ArrayStagingMemory
    );

    // Map the buffer permanently
    vkMapMemory(m_state->device, m_mat4ArrayStagingMemory, 0, bufferSize, 0, &m_mat4ArrayMappedData);
    m_mat4ArrayBufferSize = bufferSize;

    // Initialize buffer to identity matrices (prevents garbage data for unused bone slots)
    // This is important because the shader may access any bone index up to MAX_BONES
    if (m_mat4ArrayMappedData) {
        float* ptr = static_cast<float*>(m_mat4ArrayMappedData);
        for (size_t i = 0; i < MAX_BONE_MATRICES; ++i) {
            // Identity matrix: 1 on diagonal, 0 elsewhere
            // Column-major: [col0, col1, col2, col3]
            // col0 = [1,0,0,0], col1 = [0,1,0,0], col2 = [0,0,1,0], col3 = [0,0,0,1]
            size_t baseIdx = i * 16;
            for (size_t j = 0; j < 16; ++j) {
                // Diagonal elements are at indices 0, 5, 10, 15
                ptr[baseIdx + j] = (j == 0 || j == 5 || j == 10 || j == 15) ? 1.0f : 0.0f;
            }
        }
    }
}

void GfxCommandListVulkan::ensureMaterialUBO() {
    if (m_materialUBORingBuffer != VK_NULL_HANDLE) {
        return;  // Already created
    }

    // Create material UBO ring buffer - one slot per draw call within a frame.
    // Each slot must be aligned to minUniformBufferOffsetAlignment.
    size_t alignment = m_state->minUniformBufferOffsetAlignment;
    size_t alignedMaterialSize = ((MATERIAL_UBO_SIZE + alignment - 1) / alignment) * alignment;
    size_t totalBufferSize = alignedMaterialSize * MAX_MATERIAL_UBOS_PER_FRAME;

    m_state->createBuffer(
        totalBufferSize,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        m_materialUBORingBuffer,
        m_materialUBORingMemory
    );

    // Map the buffer permanently
    vkMapMemory(m_state->device, m_materialUBORingMemory, 0, totalBufferSize, 0, &m_materialUBORingMappedData);

    m_materialUBOIndex = 0;
}

void GfxCommandListVulkan::resetMaterialData() {
    // Start a new batch with zeroed material bytes. The engine re-supplies the
    // material uniforms it needs (per batch and per item), each written at the
    // offset the bound shader reflected for it. Any field the engine does not set
    // reads as zero, matching the DX12 backend's per-batch reset behaviour.
    //
    // NOTE: We do NOT clear m_textures here. Textures bound at batch setup persist
    // for every item in the batch; per-item overrides replace specific bindings.
    if (m_materialData.size() != MATERIAL_UBO_SIZE) {
        m_materialData.assign(MATERIAL_UBO_SIZE, 0);
    } else {
        std::fill(m_materialData.begin(), m_materialData.end(), uint8_t(0));
    }
}

void GfxCommandListVulkan::setUniformColor(const char* name, const Color& value) {
    setUniformVec4(name, Vec4(value.r, value.g, value.b, value.a));
}

void GfxCommandListVulkan::draw(uint32_t vertexCount, uint32_t instanceCount,
                                 uint32_t firstVertex, uint32_t firstInstance) {

    // Ensure render pass is active
    if (!m_renderPassActive && m_currentRenderTargetHandle.isValid()) {
        auto it = m_state->renderTargets.find(m_currentRenderTargetHandle.id);
        if (it != m_state->renderTargets.end()) {
            beginRenderPass(&it->second, m_clearColor, m_clearDepth);
        } else {
            
        }
    }

    if (!m_renderPassActive) {
        
        return;
    }

    // Flush descriptor sets before drawing (update if dirty, then bind)
    flushDescriptorSets();

    vkCmdDraw(m_commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);

    // Do NOT clear bound textures here: textures bound at batch setup (material
    // albedo, shadow maps, ...) must persist for every item in the batch. The
    // engine re-binds base textures itself after any per-item texture override.
    // Mark dirty so the next draw item gets its own descriptor set + material slot.
    m_descriptorSetsDirty = true;
}

void GfxCommandListVulkan::drawIndexed(uint32_t indexCount, uint32_t instanceCount,
                                        uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) {
    // Validate index buffer is bound and has enough indices
    if (!m_indexBufferBound) {
        
        return;
    }

    // Calculate required buffer size and validate
    uint32_t indexSize = (m_currentIndexFormat == IndexFormat::UInt16) ? 2 : 4;
    uint64_t requiredSize = static_cast<uint64_t>(firstIndex + indexCount) * indexSize;
    if (requiredSize > m_boundIndexBufferSize) {
        
        return;
    }

    // Ensure render pass is active
    if (!m_renderPassActive && m_currentRenderTargetHandle.isValid()) {
        auto it = m_state->renderTargets.find(m_currentRenderTargetHandle.id);
        if (it != m_state->renderTargets.end()) {
            beginRenderPass(&it->second, m_clearColor, m_clearDepth);
        } else {

        }
    }

    if (!m_renderPassActive) {

        return;
    }

    // Flush descriptor sets before drawing (update if dirty, then bind)
    flushDescriptorSets();

    vkCmdDrawIndexed(m_commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);

    // Do NOT clear bound textures here (see draw()): batch-level textures must
    // persist across every item; the engine re-binds base textures after any
    // per-item texture override. Mark dirty so the next item gets its own
    // descriptor set + material ring slot.
    m_descriptorSetsDirty = true;
}

void GfxCommandListVulkan::barrier() {
    VkMemoryBarrier memoryBarrier{};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memoryBarrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    memoryBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;

    vkCmdPipelineBarrier(m_commandBuffer,
                         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                         0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);
}

void GfxCommandListVulkan::beginDebugMarker(const char* name) {
    // Check for VK_EXT_debug_utils
    static auto vkCmdBeginDebugUtilsLabelEXT = (PFN_vkCmdBeginDebugUtilsLabelEXT)
        vkGetInstanceProcAddr(m_state->instance, "vkCmdBeginDebugUtilsLabelEXT");

    if (vkCmdBeginDebugUtilsLabelEXT) {
        VkDebugUtilsLabelEXT label{};
        label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
        label.pLabelName = name;
        label.color[0] = 1.0f;
        label.color[1] = 1.0f;
        label.color[2] = 1.0f;
        label.color[3] = 1.0f;
        vkCmdBeginDebugUtilsLabelEXT(m_commandBuffer, &label);
    }
}

void GfxCommandListVulkan::endDebugMarker() {
    static auto vkCmdEndDebugUtilsLabelEXT = (PFN_vkCmdEndDebugUtilsLabelEXT)
        vkGetInstanceProcAddr(m_state->instance, "vkCmdEndDebugUtilsLabelEXT");

    if (vkCmdEndDebugUtilsLabelEXT) {
        vkCmdEndDebugUtilsLabelEXT(m_commandBuffer);
    }
}

} // namespace lupine

#endif // LUPINE_HAS_VULKAN
