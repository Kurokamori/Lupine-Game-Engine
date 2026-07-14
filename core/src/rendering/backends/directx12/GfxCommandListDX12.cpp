#include "lupine/rendering/backends/directx12/GfxCommandListDX12.hpp"

#ifdef LUPINE_HAS_DIRECTX12

#include "lupine/rendering/backends/GfxDeviceDX12.hpp"
#include "lupine/logger/Logger.hpp"
#include <algorithm>
#include <cstring>

namespace lupine {

GfxCommandListDX12::GfxCommandListDX12(GfxDeviceDX12* device, DirectX12State* state)
    : m_device(device)
    , m_state(state)
    , m_commandList(state->commandList.Get())
    , m_materialData(MATERIAL_CB_SIZE, 0)
    , m_boneData(BONE_CB_SIZE, 0)
{
}

GfxCommandListDX12::~GfxCommandListDX12() {
    if (m_isRendering) {
        endRendering();
    }
}

void GfxCommandListDX12::setRenderTarget(RenderTargetHandle target) {

    m_currentRenderTargetHandle = target;

    if (!target.isValid()) {
        
        m_currentRenderTarget = nullptr;
        return;
    }

    auto it = m_state->renderTargets.find(target.id);
    if (it == m_state->renderTargets.end()) {
        LOG_ERROR(LogCategory::Render, "[DX12] setRenderTarget: render target {} not found in map! Map has {} entries",
                  target.id, m_state->renderTargets.size());
        return;
    }

    m_currentRenderTarget = &it->second;
    m_state->currentRenderTargetId = target.id;
    beginRendering(m_currentRenderTarget);
}

void GfxCommandListDX12::setViewport(const Viewport& viewport) {
    D3D12_VIEWPORT vp = {};
    vp.TopLeftX = viewport.x;
    vp.TopLeftY = viewport.y;
    vp.Width = viewport.width;
    vp.Height = viewport.height;
    vp.MinDepth = viewport.minDepth;
    vp.MaxDepth = viewport.maxDepth;

    m_commandList->RSSetViewports(1, &vp);
}

void GfxCommandListDX12::setScissor(const ScissorRect& scissor) {
    D3D12_RECT rect = {};

    // If scissor has zero dimensions, it means "no scissor" - use a large rect
    // This effectively disables scissoring since DX12 always has scissor test enabled
    if (scissor.width == 0 || scissor.height == 0) {
        rect.left = 0;
        rect.top = 0;
        rect.right = 16384;   // Large enough for any framebuffer
        rect.bottom = 16384;
    } else {
        rect.left = static_cast<LONG>(scissor.x);
        rect.top = static_cast<LONG>(scissor.y);
        rect.right = static_cast<LONG>(scissor.x + scissor.width);
        rect.bottom = static_cast<LONG>(scissor.y + scissor.height);
    }

    m_commandList->RSSetScissorRects(1, &rect);
}

void GfxCommandListDX12::clearColor(const Color& color) {
    m_clearColor = color;
    m_pendingColorClear = true;
}

void GfxCommandListDX12::clearDepth(float depth) {
    m_clearDepth = depth;
    m_pendingDepthClear = true;
}

void GfxCommandListDX12::clearStencil(uint8_t stencil) {
    m_clearStencil = stencil;
    m_pendingStencilClear = true;
}

void GfxCommandListDX12::bindPipeline(PipelineHandle pipeline) {
    m_currentPipeline = pipeline;

    // Clear uniform location cache when switching pipelines
    m_uniformLocationCache.clear();

    // Zero out material data
    std::memset(m_materialData.data(), 0, MATERIAL_CB_SIZE);

    // Initialize the standard PerObjectUniforms prefix (352 bytes).
    // All DX12 shaders use this prefix — the transpiler always emits it.
    float identityMatrix[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    std::memcpy(m_materialData.data() + 0, identityMatrix, sizeof(identityMatrix));    // u_ViewProjection
    std::memcpy(m_materialData.data() + 64, identityMatrix, sizeof(identityMatrix));   // u_Model
    std::memcpy(m_materialData.data() + 128, identityMatrix, sizeof(identityMatrix));  // u_NormalMatrix

    float defaultTintColor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    std::memcpy(m_materialData.data() + 192, defaultTintColor, sizeof(defaultTintColor));

    float defaultUVRect[4] = {0.0f, 0.0f, 1.0f, 1.0f};
    std::memcpy(m_materialData.data() + 224, defaultUVRect, sizeof(defaultUVRect));

    // Dynamic allocation starts after the 352-byte prefix so that
    // shader-specific uniforms (u_View, u_SkyboxType, etc.) don't
    // collide with the prefix.  executeBatch writes the full prefix
    // via pushConstants, which would overwrite anything at offsets < 352.
    m_nextUniformOffset = 352;

    m_materialDataDirty = true;

    if (!pipeline.isValid()) {

        return;
    }

    auto it = m_state->pipelines.find(pipeline.id);
    if (it == m_state->pipelines.end()) {
        LOG_ERROR(LogCategory::Render, "[DX12] bindPipeline: pipeline {} not found", pipeline.id);
        return;
    }

    DX12Pipeline& dx12Pipeline = it->second;

    // Set pipeline state
    m_commandList->SetPipelineState(dx12Pipeline.pipelineState.Get());
    m_commandList->SetGraphicsRootSignature(dx12Pipeline.rootSignature.rootSignature.Get());
    m_commandList->IASetPrimitiveTopology(dx12Pipeline.d3dTopology);

    // SetGraphicsRootSignature invalidates ALL root parameters, including the
    // SRV descriptor table and sampler table.  We MUST mark descriptors dirty so
    // that applyDescriptors() re-binds the table and re-checks texture states
    // (critical for shadow maps that alternate between DEPTH_WRITE and PSR).
    //
    // NOTE: We do NOT clear m_boundTextures here.  Stale textures from previous
    // batches are harmless — applyDescriptors will validate each one and use null
    // SRVs for any that are invalid.  Clearing would leave the descriptor table
    // uninitialized if the new pipeline's shader doesn't explicitly bind textures
    // (e.g. shadow shaders), which can cause GPU page faults.
    m_descriptorsDirty = true;
    m_srvTableDirty = true;
    m_vertexBuffersDirty = true;

    // Bind default CBVs for root params 1-3 so they always have valid GPU addresses,
    // preventing page faults if applyDescriptors doesn't rebind them all.
    if (m_state->defaultCBV1GPUAddress != 0) {
        m_commandList->SetGraphicsRootConstantBufferView(1, m_state->defaultCBV1GPUAddress);
    }
    if (m_state->defaultCBV2GPUAddress != 0) {
        m_commandList->SetGraphicsRootConstantBufferView(2, m_state->defaultCBV2GPUAddress);
    }
    if (m_state->defaultCBV3GPUAddress != 0) {
        m_commandList->SetGraphicsRootConstantBufferView(3, m_state->defaultCBV3GPUAddress);
    }

    m_state->currentPipeline = &dx12Pipeline;
}

void GfxCommandListDX12::setLineWidth(float) {
    // DirectX 12 does not support configurable line width
    // Line width is always 1 pixel
}

void GfxCommandListDX12::bindVertexBuffer(BufferHandle buffer, uint32_t binding, uint64_t offset) {
    if (!buffer.isValid()) {
        return;
    }

    // Ensure vertex buffer array is large enough
    if (binding >= m_vertexBuffers.size()) {
        m_vertexBuffers.resize(binding + 1);
    }

    m_vertexBuffers[binding] = {buffer, offset};
    m_vertexBuffersDirty = true;
}

void GfxCommandListDX12::bindIndexBuffer(BufferHandle buffer, IndexFormat format, uint64_t offset) {
    m_currentIndexBuffer = buffer;
    m_currentIndexFormat = format;
    m_indexBufferOffset = offset;

    if (!buffer.isValid()) {
        
        return;
    }

    auto it = m_state->buffers.find(buffer.id);
    if (it == m_state->buffers.end()) {
        LOG_ERROR(LogCategory::Render, "[DX12] bindIndexBuffer: buffer {} not found", buffer.id);
        return;
    }

    DX12Buffer& dx12Buffer = it->second;

    D3D12_INDEX_BUFFER_VIEW ibv = {};
    ibv.BufferLocation = dx12Buffer.gpuAddress + offset;
    ibv.SizeInBytes = static_cast<UINT>(dx12Buffer.size - offset);
    ibv.Format = (format == IndexFormat::UInt16) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;

    m_commandList->IASetIndexBuffer(&ibv);
}

void GfxCommandListDX12::bindUniformBuffer(UniformBufferHandle buffer, uint32_t binding, uint32_t) {
    if (!buffer.isValid() || binding >= MAX_BOUND_CBVS) {
        return;
    }

    m_boundCBVs[binding] = buffer;
    m_descriptorsDirty = true;
}

void GfxCommandListDX12::bindTexture(TextureHandle texture, uint32_t binding, uint32_t) {
    if (binding >= MAX_BOUND_TEXTURES) {
        return;
    }

    // Only mark SRV table dirty if the texture actually changed
    if (m_boundTextures[binding].id != texture.id) {
        m_boundTextures[binding] = texture;
        m_srvTableDirty = true;
    }
    m_descriptorsDirty = true;
}

void GfxCommandListDX12::bindSampler(SamplerHandle sampler, uint32_t binding, uint32_t) {
    if (binding >= MAX_BOUND_SAMPLERS) {
        return;
    }

    m_boundSamplers[binding] = sampler;
    m_descriptorsDirty = true;
}

void GfxCommandListDX12::pushConstants(ShaderStage, const void* data, uint32_t size, uint32_t offset) {
    if (offset + size > MATERIAL_CB_SIZE) {
        LOG_ERROR(LogCategory::Render, "[DX12] Push constants exceed material buffer size");
        return;
    }

    std::memcpy(m_materialData.data() + offset, data, size);
    m_materialDataDirty = true;

    uint32_t endOffset = offset + size;
    if (endOffset > m_nextUniformOffset) {
        m_nextUniformOffset = endOffset;
    }
}

void GfxCommandListDX12::setUniformFloat(const char* name, float value) {
    uint32_t offset = getUniformLocation(name, sizeof(float));
    if (offset + sizeof(float) <= MATERIAL_CB_SIZE) {
        std::memcpy(m_materialData.data() + offset, &value, sizeof(float));
        m_materialDataDirty = true;
    }
}

void GfxCommandListDX12::setUniformInt(const char* name, int value) {
    uint32_t offset = getUniformLocation(name, sizeof(int));

    // DEBUG: Log skybox type uniform to diagnose black skybox issue
    if (std::strcmp(name, "u_SkyboxType") == 0) {
        
    }

    if (offset + sizeof(int) <= MATERIAL_CB_SIZE) {
        std::memcpy(m_materialData.data() + offset, &value, sizeof(int));
        m_materialDataDirty = true;
    }
}

void GfxCommandListDX12::setUniformBool(const char* name, bool value) {
    int intValue = value ? 1 : 0;
    uint32_t offset = getUniformLocation(name, sizeof(int));
    if (offset + sizeof(int) <= MATERIAL_CB_SIZE) {
        std::memcpy(m_materialData.data() + offset, &intValue, sizeof(int));
        m_materialDataDirty = true;
    }
}

void GfxCommandListDX12::setUniformVec2(const char* name, const Vec2& value) {
    uint32_t offset = getUniformLocation(name, sizeof(Vec2));
    if (offset + sizeof(Vec2) <= MATERIAL_CB_SIZE) {
        std::memcpy(m_materialData.data() + offset, &value, sizeof(Vec2));
        m_materialDataDirty = true;
    }
}

void GfxCommandListDX12::setUniformVec3(const char* name, const Vec3& value) {
    // Vec3 needs to be aligned to 16 bytes in HLSL
    uint32_t offset = getUniformLocation(name, 16);
    if (offset + sizeof(Vec3) <= MATERIAL_CB_SIZE) {
        std::memcpy(m_materialData.data() + offset, &value, sizeof(Vec3));
        m_materialDataDirty = true;
    }
}

void GfxCommandListDX12::setUniformVec4(const char* name, const Vec4& value) {
    uint32_t offset = getUniformLocation(name, sizeof(Vec4));
    if (offset + sizeof(Vec4) <= MATERIAL_CB_SIZE) {
        std::memcpy(m_materialData.data() + offset, &value, sizeof(Vec4));
        m_materialDataDirty = true;
    }
}

void GfxCommandListDX12::setUniformMat4(const char* name, const math::Mat4& value) {
    uint32_t offset = getUniformLocation(name, sizeof(math::Mat4));
    if (offset + sizeof(math::Mat4) <= MATERIAL_CB_SIZE) {
        std::memcpy(m_materialData.data() + offset, &value, sizeof(math::Mat4));
        m_materialDataDirty = true;
    }
}

void GfxCommandListDX12::setUniformMat4Array(const char* name, const math::Mat4* values, size_t count) {
    // Check if this is bone transform data
    if (std::strcmp(name, "u_BoneTransforms") == 0) {
        size_t boneCount = std::min(count, static_cast<size_t>(MAX_BONES));
        uint32_t totalSize = static_cast<uint32_t>(sizeof(math::Mat4) * boneCount);

        if (totalSize <= BONE_CB_SIZE) {
            std::memcpy(m_boneData.data(), values, totalSize);
            m_boneDataDirty = true;
        }
        return;
    }

    // Regular mat4 array
    uint32_t totalSize = static_cast<uint32_t>(sizeof(math::Mat4) * count);
    uint32_t offset = getUniformLocation(name, totalSize);
    if (offset + totalSize <= MATERIAL_CB_SIZE) {
        std::memcpy(m_materialData.data() + offset, values, totalSize);
        m_materialDataDirty = true;
    }
}

void GfxCommandListDX12::setUniformColor(const char* name, const Color& value) {
    uint32_t offset = getUniformLocation(name, sizeof(Color));
    if (offset + sizeof(Color) <= MATERIAL_CB_SIZE) {
        std::memcpy(m_materialData.data() + offset, &value, sizeof(Color));
        m_materialDataDirty = true;
    }
}

void GfxCommandListDX12::resetMaterialData() {
    std::memset(m_materialData.data(), 0, MATERIAL_CB_SIZE);
    std::memset(m_boneData.data(), 0, BONE_CB_SIZE);
    m_uniformLocationCache.clear();
    // Dynamic uniform allocation must never start below the 352-byte
    // PerObjectUniforms prefix — executeBatch rewrites offsets < 352
    // via pushConstants on every draw (see bindPipeline).
    m_nextUniformOffset = 352;
    m_materialDataDirty = true;
    m_boneDataDirty = false;
}

void GfxCommandListDX12::draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) {
    if (!m_state->currentPipeline) {
        LOG_ERROR(LogCategory::Render, "[DX12] draw: No pipeline bound!");
        return;
    }

    // Check device health
    if (m_state->deviceLost) {
        return;
    }
    if (m_state->device->GetDeviceRemovedReason() != S_OK) {
        m_state->deviceLost = true;
        return;
    }

    applyPendingClears();
    applyVertexBuffers();
    applyDescriptors();

    if (m_srvPoolExhausted) {
        return;
    }

    // Update constant buffers - skip draw if allocation fails to prevent GPU hang
    if (!updateMaterialConstantBuffer() || !updateBoneConstantBuffer()) {
        return;  // Heap exhausted, skip this draw
    }

    m_commandList->DrawInstanced(vertexCount, instanceCount, firstVertex, firstInstance);
}

void GfxCommandListDX12::drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) {
    // Per-frame draw counter (reset in beginRendering or via frame fence)
    static uint32_t s_drawCallCounter = 0;
    static uint64_t s_lastFenceValue = 0;

    // Reset counter when we see a new fence value (new frame)
    if (m_state->fenceValue != s_lastFenceValue) {
        s_drawCallCounter = 0;
        s_lastFenceValue = m_state->fenceValue;
    }

    uint32_t thisDrawCall = ++s_drawCallCounter;

    if (!m_state->currentPipeline) {
        LOG_ERROR(LogCategory::Render, "[DX12] drawIndexed: No pipeline bound!");
        return;
    }

    // Stop rendering immediately if the device was lost.
    // Use the authoritative deviceLost flag on DirectX12State (not a disconnected static)
    // so that device recovery / reinit can clear it in the future.
    if (m_state->deviceLost) {
        return;
    }
    HRESULT deviceStatus = m_state->device->GetDeviceRemovedReason();
    if (deviceStatus != S_OK) {
        LOG_ERROR(LogCategory::Render, "[DX12] Device lost (0x{:08X}) at draw #{}. Stopping all rendering.",
                  static_cast<unsigned int>(deviceStatus), thisDrawCall);

        // Log bound state at crash for diagnosis
        if (m_state->currentPipeline) {
            LOG_ERROR(LogCategory::Render, "[DX12] Active pipeline: stride={} bytes, {} vertex attrs",
                      m_state->currentPipeline->vertexLayout.stride,
                      m_state->currentPipeline->vertexLayout.attributes.size());
        }

        // Log bound textures
        for (uint32_t i = 0; i < MAX_BOUND_TEXTURES; ++i) {
            if (m_boundTextures[i].isValid()) {
                auto texIt = m_state->textures.find(m_boundTextures[i].id);
                if (texIt != m_state->textures.end()) {
                    LOG_ERROR(LogCategory::Render, "[DX12] Bound texture slot {}: id={}, srvIdx={}, state={}, resource={}",
                              i, m_boundTextures[i].id, texIt->second.srvIndex,
                              static_cast<int>(texIt->second.currentState),
                              (void*)texIt->second.resource.Get());
                } else {
                    LOG_ERROR(LogCategory::Render, "[DX12] Bound texture slot {}: id={} NOT FOUND in texture map!", i, m_boundTextures[i].id);
                }
            }
        }

        // Log bound CBVs
        for (uint32_t i = 0; i < MAX_BOUND_CBVS; ++i) {
            if (m_boundCBVs[i].isValid()) {
                auto ubIt = m_state->uniformBuffers.find(m_boundCBVs[i].id);
                if (ubIt != m_state->uniformBuffers.end()) {
                    LOG_ERROR(LogCategory::Render, "[DX12] Bound CBV slot {}: id={}, gpuAddr=0x{:016X}",
                              i, m_boundCBVs[i].id, ubIt->second.gpuAddress);
                } else {
                    LOG_ERROR(LogCategory::Render, "[DX12] Bound CBV slot {}: id={} NOT FOUND!", i, m_boundCBVs[i].id);
                }
            }
        }

        // Log upload heap usage
        LOG_ERROR(LogCategory::Render, "[DX12] Upload heap: frame={}, SRV pool index={}",
                  m_state->currentFrameIndex, m_state->srvTablePoolIndex);

        m_state->deviceLost = true;
        return;
    }

    m_srvPoolExhausted = false;

    applyPendingClears();
    applyVertexBuffers();
    applyDescriptors();

    // Skip draw entirely if SRV pool exhausted (drawing with stale descriptors causes GPU faults)
    if (m_srvPoolExhausted) {
        return;
    }

    // Update constant buffers - skip draw if allocation fails to prevent GPU hang
    if (!updateMaterialConstantBuffer() || !updateBoneConstantBuffer()) {
        return;  // Heap exhausted, skip this draw
    }

    m_commandList->DrawIndexedInstanced(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void GfxCommandListDX12::barrier() {
    // Insert UAV barrier (resource barrier for unordered access)
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.UAV.pResource = nullptr;  // All UAV resources

    m_commandList->ResourceBarrier(1, &barrier);
}

void GfxCommandListDX12::beginDebugMarker(const char* name) {
    if (!m_commandList) {
        return;
    }

    // PIX_EVENT_UNICODE_VERSION: marker payload is a null-terminated UTF-16 string.
    // Using the documented encoding directly avoids a hard dependency on
    // WinPixEventRuntime while still being recognized by PIX and RenderDoc.
    constexpr UINT PIX_EVENT_UNICODE_VERSION = 2;

    const char* label = name ? name : "";
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, label, -1, nullptr, 0);
    if (wideLen <= 0) {
        return;
    }

    std::vector<wchar_t> wideLabel(static_cast<size_t>(wideLen));
    MultiByteToWideChar(CP_UTF8, 0, label, -1, wideLabel.data(), wideLen);
    m_commandList->BeginEvent(PIX_EVENT_UNICODE_VERSION, wideLabel.data(),
                              static_cast<UINT>(wideLen) * static_cast<UINT>(sizeof(wchar_t)));
    ++m_debugMarkerDepth;
}

void GfxCommandListDX12::endDebugMarker() {
    // Only emit EndEvent when a matching BeginEvent was actually recorded on this command
    // list. beginDebugMarker can return early (null command list or label-encoding failure)
    // without opening a marker; emitting EndEvent unconditionally in that case would make
    // the queue's PixEndEvent count exceed PixBeginEvent and trip DX12 validation.
    if (m_commandList && m_debugMarkerDepth > 0) {
        m_commandList->EndEvent();
        --m_debugMarkerDepth;
    }
}

void GfxCommandListDX12::closeOpenDebugMarkers() {
    if (!m_commandList) {
        m_debugMarkerDepth = 0;
        return;
    }
    while (m_debugMarkerDepth > 0) {
        m_commandList->EndEvent();
        --m_debugMarkerDepth;
    }
}

void GfxCommandListDX12::beginRendering(DX12RenderTarget* target) {

    if (!target) {
        LOG_ERROR(LogCategory::Render, "[DX12] beginRendering: target is null!");
        return;
    }

    // If rendering to a different target, end the current render pass first
    if (m_isRendering && m_currentRenderTarget != target) {
        
        endRendering();
    }

    m_isRendering = true;

    // Transition render target resources to render target state
    transitionRenderTarget(target, true);

    // Set render targets
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = {};
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = {};
    D3D12_CPU_DESCRIPTOR_HANDLE* pDsvHandle = nullptr;

    if (target->rtvIndex != UINT32_MAX) {
        rtvHandle = m_state->rtvHeap.getCPUHandle(target->rtvIndex);
    }

    if (target->dsvIndex != UINT32_MAX) {
        dsvHandle = m_state->dsvHeap.getCPUHandle(target->dsvIndex);
        pDsvHandle = &dsvHandle;
    }

    m_commandList->OMSetRenderTargets(
        target->rtvIndex != UINT32_MAX ? 1 : 0,
        target->rtvIndex != UINT32_MAX ? &rtvHandle : nullptr,
        FALSE,
        pDsvHandle
    );

    // Set default viewport and scissor - DirectX 12 requires both to be set
    D3D12_VIEWPORT vp = {};
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    vp.Width = static_cast<float>(target->width);
    vp.Height = static_cast<float>(target->height);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    m_commandList->RSSetViewports(1, &vp);

    D3D12_RECT scissor = {};
    scissor.left = 0;
    scissor.top = 0;
    scissor.right = static_cast<LONG>(target->width);
    scissor.bottom = static_cast<LONG>(target->height);
    m_commandList->RSSetScissorRects(1, &scissor);

    // Apply pending clears
    applyPendingClears();
}

void GfxCommandListDX12::endRendering() {

    if (!m_isRendering || !m_currentRenderTarget) {
        
        m_isRendering = false;
        return;
    }

    // Apply any pending clears that haven't been applied yet
    // (in case no draw commands were issued after clearColor/clearDepth)
    applyPendingClears();

    // Transition render target back to common/shader resource state
    transitionRenderTarget(m_currentRenderTarget, false);

    m_isRendering = false;
}

void GfxCommandListDX12::transitionRenderTarget(DX12RenderTarget* target, bool toRenderTarget) {
    if (!target) return;

    // Transition color texture
    if (target->colorTextureHandle.isValid()) {
        auto texIt = m_state->textures.find(target->colorTextureHandle.id);
        if (texIt != m_state->textures.end()) {
            D3D12_RESOURCE_STATES before = texIt->second.currentState;
            D3D12_RESOURCE_STATES after = toRenderTarget ?
                D3D12_RESOURCE_STATE_RENDER_TARGET :
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

            if (before != after) {
                m_state->transitionResource(texIt->second.resource.Get(), before, after);
                texIt->second.currentState = after;
            }
        }
    } else if (target->isSwapchainBackbuffer && target->owningSwapchainId != 0) {
        // For swapchain backbuffers, transition the current backbuffer using tracked state
        auto scIt = m_state->swapchains.find(target->owningSwapchainId);
        if (scIt != m_state->swapchains.end()) {
            DX12Swapchain& sc = scIt->second;
            ID3D12Resource* backbuffer = sc.backbufferTextures[sc.currentBackbufferIndex].Get();

            // Get actual current state from tracking
            D3D12_RESOURCE_STATES before = sc.backbufferStates[sc.currentBackbufferIndex];
            D3D12_RESOURCE_STATES after = toRenderTarget ?
                D3D12_RESOURCE_STATE_RENDER_TARGET :
                D3D12_RESOURCE_STATE_PRESENT;

            // Only transition if state actually changed
            if (before != after) {
                
                m_state->transitionResource(backbuffer, before, after);
                sc.backbufferStates[sc.currentBackbufferIndex] = after;
            }
        }
    }

    // Transition depth texture between DEPTH_WRITE and PIXEL_SHADER_RESOURCE.
    // Without this, shadow map depth textures left in PIXEL_SHADER_RESOURCE after
    // sampling would cause a resource state violation on the next shadow render pass,
    // leading to GPU page faults and device hung (0x887A0006).
    if (target->depthTextureHandle.isValid()) {
        auto depthIt = m_state->textures.find(target->depthTextureHandle.id);
        if (depthIt != m_state->textures.end()) {
            D3D12_RESOURCE_STATES before = depthIt->second.currentState;
            D3D12_RESOURCE_STATES after = toRenderTarget ?
                D3D12_RESOURCE_STATE_DEPTH_WRITE :
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

            if (before != after) {
                m_state->transitionResource(depthIt->second.resource.Get(), before, after);
                depthIt->second.currentState = after;
            }
        } else {
            LOG_ERROR(LogCategory::Render, "[DX12] transitionRenderTarget: depth texture id={} not found in texture map!",
                      target->depthTextureHandle.id);
        }
    }
}

bool GfxCommandListDX12::updateMaterialConstantBuffer() {
    if (!m_materialDataDirty || !m_state->currentPipeline) {
        return true;  // Nothing to do, but OK to proceed
    }

    // Calculate actual size needed (aligned to 256 bytes for D3D12 CBV requirements)
    // Use m_nextUniformOffset which tracks how much data was actually written,
    // with a minimum of 512 bytes to ensure we have enough space for standard uniforms
    uint32_t actualSize = std::max(m_nextUniformOffset, 512u);
    actualSize = static_cast<uint32_t>(DX12Utils::align(actualSize, 256));

    // Allocate only what we need in upload heap (instead of full 64KB)
    DX12FrameResources& frame = m_state->frameResources[m_state->currentFrameIndex];
    auto alloc = frame.uploadHeap.allocate(actualSize, 256);

    if (!alloc.cpuAddress) {
        // Allocation failed - skip this draw to avoid GPU hang.
        // The per-frame warning is now handled inside UploadHeap::allocate().
        return false;
    }

    std::memcpy(alloc.cpuAddress, m_materialData.data(), actualSize);

    // Set root CBV
    if (m_state->currentPipeline->rootSignature.cbvRootParamIndex >= 0) {
        m_commandList->SetGraphicsRootConstantBufferView(
            static_cast<UINT>(m_state->currentPipeline->rootSignature.cbvRootParamIndex),
            alloc.gpuAddress
        );
    }

    m_materialDataDirty = false;
    return true;
}

bool GfxCommandListDX12::updateBoneConstantBuffer() {
    if (!m_state->currentPipeline) {
        return true;
    }

    // Check if this is a skeletal pipeline (has bone attributes)
    bool isSkeletalPipeline = m_state->currentPipeline->vertexLayout.attributes.size() >= 7;

    // Only bind bone CBV for skeletal pipelines or when bone data is dirty
    if (m_boneDataDirty) {
        // Allocate from upload heap and copy bone data
        uint32_t boneAllocSize = static_cast<uint32_t>(DX12Utils::align(BONE_CB_SIZE, 256));

        DX12FrameResources& frame = m_state->frameResources[m_state->currentFrameIndex];
        auto alloc = frame.uploadHeap.allocate(boneAllocSize, 256);

        if (!alloc.cpuAddress) {
            // Allocation failed - use default buffer for skeletal pipelines
            if (isSkeletalPipeline && m_state->defaultCBV2GPUAddress != 0) {
                m_commandList->SetGraphicsRootConstantBufferView(2, m_state->defaultCBV2GPUAddress);
            }
            return true;
        }

        std::memcpy(alloc.cpuAddress, m_boneData.data(), boneAllocSize);
        m_commandList->SetGraphicsRootConstantBufferView(2, alloc.gpuAddress);
        m_boneDataDirty = false;
    } else if (isSkeletalPipeline) {
        // Skeletal pipeline but no bone data - bind default buffer
        // This prevents the shader from reading uninitialized memory at b2
        if (m_state->defaultCBV2GPUAddress != 0) {
            m_commandList->SetGraphicsRootConstantBufferView(2, m_state->defaultCBV2GPUAddress);
        }
    }
    // For non-skeletal pipelines without bone data, don't bind anything to b2

    return true;
}

void GfxCommandListDX12::applyPendingClears() {
    if (!m_currentRenderTarget) {
        return;
    }

    if (m_pendingColorClear && m_currentRenderTarget->rtvIndex != UINT32_MAX) {
        float clearColorArray[4] = {m_clearColor.r, m_clearColor.g, m_clearColor.b, m_clearColor.a};
        
        m_commandList->ClearRenderTargetView(
            m_state->rtvHeap.getCPUHandle(m_currentRenderTarget->rtvIndex),
            clearColorArray,
            0,
            nullptr
        );
        m_pendingColorClear = false;
    }

    if ((m_pendingDepthClear || m_pendingStencilClear) && m_currentRenderTarget->dsvIndex != UINT32_MAX) {
        D3D12_CLEAR_FLAGS clearFlags = static_cast<D3D12_CLEAR_FLAGS>(0);
        if (m_pendingDepthClear) clearFlags = static_cast<D3D12_CLEAR_FLAGS>(clearFlags | D3D12_CLEAR_FLAG_DEPTH);
        if (m_pendingStencilClear) clearFlags = static_cast<D3D12_CLEAR_FLAGS>(clearFlags | D3D12_CLEAR_FLAG_STENCIL);

        m_commandList->ClearDepthStencilView(
            m_state->dsvHeap.getCPUHandle(m_currentRenderTarget->dsvIndex),
            clearFlags,
            m_clearDepth,
            m_clearStencil,
            0,
            nullptr
        );
        m_pendingDepthClear = false;
        m_pendingStencilClear = false;
    }
}

uint32_t GfxCommandListDX12::resolveVertexStrideForBinding(uint32_t binding) const {
    if (!m_state->currentPipeline) {
        return 0;
    }

    const DX12Pipeline* pipeline = m_state->currentPipeline;
    if (binding == 0) {
        return pipeline->vertexLayout.stride;
    }

    for (const auto& extraLayout : pipeline->extraVertexBuffers) {
        if (!extraLayout.attributes.empty() && extraLayout.attributes.front().binding == binding) {
            return extraLayout.stride;
        }
    }

    return pipeline->vertexLayout.stride;
}

void GfxCommandListDX12::applyVertexBuffers() {
    if (!m_vertexBuffersDirty || m_vertexBuffers.empty()) {
        return;
    }

    std::vector<D3D12_VERTEX_BUFFER_VIEW> vbViews;
    vbViews.reserve(m_vertexBuffers.size());

    for (size_t bindingIndex = 0; bindingIndex < m_vertexBuffers.size(); ++bindingIndex) {
        const auto& vb = m_vertexBuffers[bindingIndex];
        if (!vb.buffer.isValid()) {
            continue;
        }

        auto it = m_state->buffers.find(vb.buffer.id);
        if (it == m_state->buffers.end()) {
            LOG_ERROR(LogCategory::Render, "[DX12] applyVertexBuffers: buffer {} not found", vb.buffer.id);
            continue;
        }

        DX12Buffer& dx12Buffer = it->second;

        D3D12_VERTEX_BUFFER_VIEW vbv = {};
        vbv.BufferLocation = dx12Buffer.gpuAddress + vb.offset;
        vbv.SizeInBytes = static_cast<UINT>(dx12Buffer.size - vb.offset);

        // Stride is per input slot: slot 0 is the primary geometry layout;
        // higher slots come from the pipeline's extra (e.g. per-instance) layouts.
        vbv.StrideInBytes = resolveVertexStrideForBinding(static_cast<uint32_t>(bindingIndex));

        vbViews.push_back(vbv);
    }

    if (!vbViews.empty()) {
        m_commandList->IASetVertexBuffers(0, static_cast<UINT>(vbViews.size()), vbViews.data());
    }

    m_vertexBuffersDirty = false;
}

void GfxCommandListDX12::applyDescriptors() {
    if (!m_descriptorsDirty || !m_state->currentPipeline) {
        return;
    }

    // Bind uniform buffers as root CBVs
    for (uint32_t i = 0; i < MAX_BOUND_CBVS; ++i) {
        if (m_boundCBVs[i].isValid()) {
            auto it = m_state->uniformBuffers.find(m_boundCBVs[i].id);
            if (it != m_state->uniformBuffers.end()) {
                // Map binding to root parameter index
                // Root signature layout:
                //   Root param 0 = Material CBV (b0)
                //   Root param 1 = Camera CBV (b1)
                //   Root param 2 = Bone CBV (b2)
                //   Root param 3 = Light CBV (b3)
                // Binding 2 -> root param 2 (bones), Binding 3 -> root param 3 (lights)
                UINT rootIndex = UINT_MAX;
                if (i == 2) rootIndex = 2;  // Bone data at b2
                else if (i == 3) rootIndex = 3;  // Light data at b3

                if (rootIndex != UINT_MAX) {
                    m_commandList->SetGraphicsRootConstantBufferView(rootIndex, it->second.gpuAddress);
                }
            }
        }
    }

    // Bind textures via descriptor table.
    // Only allocate a NEW table from the pool when textures actually changed
    // (m_srvTableDirty).  When only CBVs/samplers changed we reuse the last table.
    bool hasTextures = false;
    for (uint32_t i = 0; i < MAX_BOUND_TEXTURES; ++i) {
        if (m_boundTextures[i].isValid()) {
            hasTextures = true;
            break;
        }
    }

    if (hasTextures && m_state->currentPipeline->rootSignature.descriptorTableIndex >= 0) {

        if (m_srvTableDirty) {
            // Textures changed — need a fresh descriptor table.
            // Allocate the pool once; if it fails, stop retrying (the heap is full).
            if (m_state->srvTablePoolStart == UINT32_MAX && !m_state->srvPoolAllocationFailed) {
                m_state->srvTablePoolStart = m_state->cbvSrvUavHeap.allocateRange(
                    DirectX12State::SRV_TABLE_SIZE * DirectX12State::SRV_TABLE_POOL_SIZE);
                if (m_state->srvTablePoolStart == UINT32_MAX) {
                    LOG_ERROR(LogCategory::Render,
                              "[DX12] Failed to allocate SRV table pool ({} descriptors). "
                              "All textured draws will be skipped.",
                              DirectX12State::SRV_TABLE_SIZE * DirectX12State::SRV_TABLE_POOL_SIZE);
                    m_state->srvPoolAllocationFailed = true;
                }
            }

            if (m_state->srvTablePoolStart != UINT32_MAX) {
                uint32_t frameSegmentOffset = m_state->currentFrameIndex * DirectX12State::SRV_TABLES_PER_FRAME;
                uint32_t tableStartIndex = m_state->srvTablePoolStart +
                    ((frameSegmentOffset + m_state->srvTablePoolIndex) * DirectX12State::SRV_TABLE_SIZE);

                if (m_state->srvTablePoolIndex >= DirectX12State::SRV_TABLES_PER_FRAME - 1) {
                    LOG_ERROR(LogCategory::Render, "[DX12] SRV table pool exhausted ({}/{} tables this frame). "
                              "Skipping remaining textured draws. Reduce unique texture combinations or draw calls.",
                              m_state->srvTablePoolIndex, DirectX12State::SRV_TABLES_PER_FRAME);
                    m_srvPoolExhausted = true;
                    m_descriptorsDirty = false;
                    return;
                }
                m_state->srvTablePoolIndex++;

                // Copy bound texture SRVs (or null SRVs) into the new table
                for (uint32_t i = 0; i < MAX_BOUND_TEXTURES && i < DirectX12State::SRV_TABLE_SIZE; ++i) {
                    D3D12_CPU_DESCRIPTOR_HANDLE srcHandle = {};
                    bool hasValidSrv = false;

                    if (m_boundTextures[i].isValid()) {
                        auto texIt = m_state->textures.find(m_boundTextures[i].id);
                        if (texIt != m_state->textures.end() && texIt->second.srvIndex != UINT32_MAX) {
                            if (texIt->second.currentState != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE &&
                                texIt->second.currentState != D3D12_RESOURCE_STATE_COMMON &&
                                texIt->second.currentState != D3D12_RESOURCE_STATE_GENERIC_READ) {
                                m_state->transitionResource(
                                    texIt->second.resource.Get(),
                                    texIt->second.currentState,
                                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
                                );
                                texIt->second.currentState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
                            }
                            srcHandle = m_state->cbvSrvUavStagingHeap.getCPUHandle(texIt->second.srvIndex);
                            hasValidSrv = true;
                        }
                    }

                    // Dimension-matched null SRV: TextureCube for slots 12-19, Texture2D otherwise
                    if (!hasValidSrv) {
                        bool isCubeSlot = (i >= 12 && i < 20) && m_state->nullSrvCubeIndex != UINT32_MAX;
                        uint32_t nullIdx = isCubeSlot ? m_state->nullSrvCubeIndex : m_state->nullSrvIndex;
                        if (nullIdx != UINT32_MAX) {
                            srcHandle = m_state->cbvSrvUavStagingHeap.getCPUHandle(nullIdx);
                            hasValidSrv = true;
                        }
                    }

                    if (hasValidSrv) {
                        D3D12_CPU_DESCRIPTOR_HANDLE dstHandle = m_state->cbvSrvUavHeap.getCPUHandle(tableStartIndex + i);
                        m_state->device->CopyDescriptorsSimple(1, dstHandle, srcHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
                    }
                }

                m_lastSrvTableGpuHandle = m_state->cbvSrvUavHeap.getGPUHandle(tableStartIndex);
            }
            m_srvTableDirty = false;
        }

        // Bind the current SRV table (new or reused from previous draw)
        if (m_lastSrvTableGpuHandle.ptr != 0) {
            m_commandList->SetGraphicsRootDescriptorTable(
                static_cast<UINT>(m_state->currentPipeline->rootSignature.descriptorTableIndex),
                m_lastSrvTableGpuHandle
            );
        }
    }

    // Bind samplers via the pre-allocated contiguous sampler table.
    // Transpiled shaders place samplers at sparse registers (e.g. s0-s3 for material,
    // s8 for shadows). We copy each bound sampler into its correct slot in the
    // pre-allocated table so the GPU always reads valid sampler descriptors.
    if (m_state->currentPipeline->rootSignature.samplerTableIndex >= 0 &&
        m_state->samplerTableStart != UINT32_MAX) {

        // Copy each bound sampler to its binding slot in the pre-allocated table
        for (uint32_t i = 0; i < MAX_BOUND_SAMPLERS && i < DirectX12State::SAMPLER_TABLE_SIZE; ++i) {
            if (m_boundSamplers[i].isValid()) {
                auto samplerIt = m_state->samplers.find(m_boundSamplers[i].id);
                if (samplerIt != m_state->samplers.end() && samplerIt->second.samplerIndex != UINT32_MAX) {
                    D3D12_CPU_DESCRIPTOR_HANDLE srcHandle = m_state->samplerStagingHeap.getCPUHandle(
                        samplerIt->second.samplerIndex);
                    D3D12_CPU_DESCRIPTOR_HANDLE dstHandle = m_state->samplerHeap.getCPUHandle(
                        m_state->samplerTableStart + i);
                    m_state->device->CopyDescriptorsSimple(
                        1, dstHandle, srcHandle, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
                }
            }
        }

        // Point the root descriptor table at the contiguous sampler block
        D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = m_state->samplerHeap.getGPUHandle(
            m_state->samplerTableStart);
        m_commandList->SetGraphicsRootDescriptorTable(
            static_cast<UINT>(m_state->currentPipeline->rootSignature.samplerTableIndex),
            gpuHandle
        );
    }

    m_descriptorsDirty = false;
}

uint32_t GfxCommandListDX12::getUniformLocation(const char* name, uint32_t size) {
    // Check cache
    auto cacheIt = m_uniformLocationCache.find(name);
    if (cacheIt != m_uniformLocationCache.end()) {
        return cacheIt->second;
    }

    // Try to look up from pipeline's shader reflection data
    if (m_state->currentPipeline) {
        auto reflIt = m_state->currentPipeline->uniformOffsets.find(name);
        if (reflIt != m_state->currentPipeline->uniformOffsets.end()) {
            uint32_t offset = reflIt->second;
            m_uniformLocationCache[name] = offset;

            uint32_t endOffset = offset + size;
            if (endOffset > m_nextUniformOffset) {
                m_nextUniformOffset = endOffset;
            }

            return offset;
        }
    }

    // Fallback: dynamically allocate offset
    // Align to 16 bytes (DX12 constant buffer alignment)
    uint32_t alignedOffset = (m_nextUniformOffset + 15) & ~15;

    if (alignedOffset + size > MATERIAL_CB_SIZE) {
        LOG_ERROR(LogCategory::Render, "[DX12] Material constant buffer overflow for uniform: {}", name);
        return 0;
    }

    m_uniformLocationCache[name] = alignedOffset;
    m_nextUniformOffset = alignedOffset + size;

    return alignedOffset;
}

} // namespace lupine

#endif // LUPINE_HAS_DIRECTX12
