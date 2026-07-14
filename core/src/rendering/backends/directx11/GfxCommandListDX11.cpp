#include "lupine/rendering/backends/directx11/GfxCommandListDX11.hpp"

#ifdef LUPINE_HAS_DIRECTX11

#include "lupine/rendering/backends/GfxDeviceDX11.hpp"
#include "lupine/logger/Logger.hpp"
#include <cstring>

namespace lupine {

GfxCommandListDX11::GfxCommandListDX11(GfxDeviceDX11* device, DirectX11State* state)
    : m_device(device)
    , m_state(state)
    , m_context(state->context.Get())
    , m_materialData(MATERIAL_CB_SIZE, 0)
    , m_boneData(BONE_CB_SIZE, 0)
{
    // Create material constant buffer
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.ByteWidth = MATERIAL_CB_SIZE;
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    HRESULT hr = m_state->device->CreateBuffer(&cbDesc, nullptr, &m_materialConstantBuffer);
    if (FAILED(hr)) {
        LOG_ERROR(LogCategory::Render, "Failed to create material constant buffer");
    }

    // Create bone constant buffer (slot 2 for skeletal animation)
    D3D11_BUFFER_DESC boneCbDesc = {};
    boneCbDesc.ByteWidth = BONE_CB_SIZE;
    boneCbDesc.Usage = D3D11_USAGE_DYNAMIC;
    boneCbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    boneCbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = m_state->device->CreateBuffer(&boneCbDesc, nullptr, &m_boneConstantBuffer);
    if (FAILED(hr)) {
        LOG_ERROR(LogCategory::Render, "Failed to create bone constant buffer");
    }
}

GfxCommandListDX11::~GfxCommandListDX11() = default;

void GfxCommandListDX11::setRenderTarget(RenderTargetHandle target) {
    m_currentRenderTargetHandle = target;

    if (!target.isValid()) {
        m_context->OMSetRenderTargets(0, nullptr, nullptr);
        m_currentRenderTarget = nullptr;
        return;
    }

    auto it = m_state->renderTargets.find(target.id);
    if (it == m_state->renderTargets.end()) {
        return;
    }

    m_currentRenderTarget = &it->second;
    beginRendering(m_currentRenderTarget);
}

void GfxCommandListDX11::setViewport(const Viewport& viewport) {
    D3D11_VIEWPORT vp = {};
    vp.TopLeftX = viewport.x;
    vp.TopLeftY = viewport.y;
    vp.Width = viewport.width;
    vp.Height = viewport.height;
    vp.MinDepth = viewport.minDepth;
    vp.MaxDepth = viewport.maxDepth;

    m_context->RSSetViewports(1, &vp);

    // Rasterizer states always enable scissor (see GfxDeviceDX11 createPipeline), so
    // default the scissor rect to the full viewport. A pass that needs clipping then
    // overrides it via setScissor; one that does not is left unclipped instead of
    // inheriting a stale or empty rect.
    D3D11_RECT scissorRect = {};
    scissorRect.left = static_cast<LONG>(viewport.x);
    scissorRect.top = static_cast<LONG>(viewport.y);
    scissorRect.right = static_cast<LONG>(viewport.x + viewport.width);
    scissorRect.bottom = static_cast<LONG>(viewport.y + viewport.height);
    m_context->RSSetScissorRects(1, &scissorRect);
}

void GfxCommandListDX11::setScissor(const ScissorRect& scissor) {
    D3D11_RECT rect = {};

    // Zero dimensions mean "no scissor" (engine-wide convention) — use a large
    // rect since D3D11 applies whatever rect is set when ScissorEnable is on.
    if (scissor.width == 0 || scissor.height == 0) {
        rect.left = 0;
        rect.top = 0;
        rect.right = 16384;
        rect.bottom = 16384;
    } else {
        rect.left = static_cast<LONG>(scissor.x);
        rect.top = static_cast<LONG>(scissor.y);
        rect.right = static_cast<LONG>(scissor.x + scissor.width);
        rect.bottom = static_cast<LONG>(scissor.y + scissor.height);
    }

    m_context->RSSetScissorRects(1, &rect);
}

void GfxCommandListDX11::clearColor(const Color& color) {
    m_clearColor = color;
    m_pendingColorClear = true;
}

void GfxCommandListDX11::clearDepth(float depth) {
    m_clearDepth = depth;
    m_pendingDepthClear = true;
}

void GfxCommandListDX11::clearStencil(uint8_t stencil) {
    m_clearStencil = stencil;
    m_pendingStencilClear = true;
}

void GfxCommandListDX11::bindPipeline(PipelineHandle pipeline) {
    m_currentPipeline = pipeline;

    // Clear uniform location cache when switching pipelines
    // Each pipeline may have different cbuffer layouts
    m_uniformLocationCache.clear();
    m_nextUniformOffset = 0;

    // Zero out material data to ensure clean state for the new pipeline
    // This prevents garbage data from previous pipelines from being read
    // by shaders with different cbuffer layouts (e.g., skybox shader reading
    // sprite data offsets would get garbage u_SkyboxType values)
    std::memset(m_materialData.data(), 0, MATERIAL_CB_SIZE);
    m_materialDataDirty = true;

    if (!pipeline.isValid()) {
        return;
    }

    auto it = m_state->pipelines.find(pipeline.id);
    if (it == m_state->pipelines.end()) {
        return;
    }

    DX11Pipeline& dx11Pipeline = it->second;

    // Bind shaders
    m_context->VSSetShader(dx11Pipeline.vertexShader.Get(), nullptr, 0);
    m_context->PSSetShader(dx11Pipeline.pixelShader.Get(), nullptr, 0);
    m_context->GSSetShader(dx11Pipeline.geometryShader.Get(), nullptr, 0);
    m_context->HSSetShader(dx11Pipeline.hullShader.Get(), nullptr, 0);
    m_context->DSSetShader(dx11Pipeline.domainShader.Get(), nullptr, 0);

    // Bind pipeline state
    m_context->IASetInputLayout(dx11Pipeline.inputLayout.Get());
    m_context->IASetPrimitiveTopology(dx11Pipeline.d3dTopology);
    m_context->RSSetState(dx11Pipeline.rasterizerState.Get());
    m_context->OMSetDepthStencilState(dx11Pipeline.depthStencilState.Get(), 0);
    m_context->OMSetBlendState(dx11Pipeline.blendState.Get(), nullptr, 0xFFFFFFFF);

    m_state->currentPipeline = &dx11Pipeline;
}

void GfxCommandListDX11::setLineWidth(float) {
    // NOTE: DirectX 11 does not support configurable line width
    // Line width is always 1 pixel in DX11
    // This is a limitation of the API
}

void GfxCommandListDX11::bindVertexBuffer(BufferHandle buffer, uint32_t binding, uint64_t offset) {
    if (!buffer.isValid()) {
        return;
    }

    auto it = m_state->buffers.find(buffer.id);
    if (it == m_state->buffers.end()) {
        return;
    }

    // Ensure vertex buffer array is large enough
    if (binding >= m_vertexBuffers.size()) {
        m_vertexBuffers.resize(binding + 1);
    }

    m_vertexBuffers[binding] = {buffer, offset};

    // Apply vertex buffers to context
    std::vector<ID3D11Buffer*> buffers;
    std::vector<UINT> strides;
    std::vector<UINT> offsets;

    for (size_t i = 0; i < m_vertexBuffers.size(); ++i) {
        const auto& vb = m_vertexBuffers[i];
        if (vb.buffer.isValid()) {
            auto vbIt = m_state->buffers.find(vb.buffer.id);
            if (vbIt != m_state->buffers.end()) {
                buffers.push_back(vbIt->second.buffer.Get());

                // Get the stride for this input slot. Slot 0 is the primary
                // geometry layout; higher slots (e.g. per-instance data) use
                // their own stride from the pipeline's per-slot stride table.
                UINT stride = 0;
                if (m_state->currentPipeline) {
                    const auto& slotStrides = m_state->currentPipeline->vertexStrides;
                    if (i < slotStrides.size()) {
                        stride = slotStrides[i];
                    } else {
                        stride = m_state->currentPipeline->vertexLayout.stride;
                    }
                }

                strides.push_back(stride);
                offsets.push_back(static_cast<UINT>(vb.offset));
            }
        }
    }

    if (!buffers.empty()) {
        // Debug: Log vertex buffer binding
        static int vbBindCount = 0;
        if (vbBindCount < 5) {
            for (size_t i = 0; i < strides.size(); ++i) {
                
            }
            vbBindCount++;
        }

        m_context->IASetVertexBuffers(0, static_cast<UINT>(buffers.size()),
                                     buffers.data(), strides.data(), offsets.data());
    }
}

void GfxCommandListDX11::bindIndexBuffer(BufferHandle buffer, IndexFormat format, uint64_t offset) {
    m_currentIndexBuffer = buffer;
    m_currentIndexFormat = format;
    m_indexBufferOffset = offset;

    if (!buffer.isValid()) {
        m_context->IASetIndexBuffer(nullptr, DXGI_FORMAT_R32_UINT, 0);
        return;
    }

    auto it = m_state->buffers.find(buffer.id);
    if (it == m_state->buffers.end()) {
        return;
    }

    DXGI_FORMAT dxgiFormat = (format == IndexFormat::UInt16) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
    m_context->IASetIndexBuffer(it->second.buffer.Get(), dxgiFormat, static_cast<UINT>(offset));
}

void GfxCommandListDX11::bindUniformBuffer(UniformBufferHandle buffer, uint32_t binding, uint32_t) {
    if (!buffer.isValid() || binding >= MAX_CONSTANT_BUFFERS) {
        
        return;
    }

    auto it = m_state->uniformBuffers.find(buffer.id);
    if (it == m_state->uniformBuffers.end()) {
        
        return;
    }

    ID3D11Buffer* cb = it->second.buffer.Get();

    // Log light UBO binding specifically (binding 3 is the light data buffer)
    if (binding == 3) {
        static int lightBindCount = 0;
        if (lightBindCount < 10) {
            
            lightBindCount++;
        }
    }

    // Bind directly to the requested slot - the shader cbuffer register matches the binding
    // Slot 0 is used for pushConstants/setUniform* data (material constant buffer)
    // Other slots (1, 2, 3, etc.) are for explicit UBOs like LightData
    m_vsConstantBuffers[binding] = cb;
    m_psConstantBuffers[binding] = cb;
    m_gsConstantBuffers[binding] = cb;
    m_hsConstantBuffers[binding] = cb;
    m_dsConstantBuffers[binding] = cb;

    m_context->VSSetConstantBuffers(binding, 1, &cb);
    m_context->PSSetConstantBuffers(binding, 1, &cb);
    m_context->GSSetConstantBuffers(binding, 1, &cb);
    m_context->HSSetConstantBuffers(binding, 1, &cb);
    m_context->DSSetConstantBuffers(binding, 1, &cb);
}

void GfxCommandListDX11::bindTexture(TextureHandle texture, uint32_t binding, uint32_t) {
    if (binding >= MAX_SHADER_RESOURCES) {
        return;
    }

    ID3D11ShaderResourceView* srv = nullptr;

    if (texture.isValid()) {
        auto it = m_state->textures.find(texture.id);
        if (it != m_state->textures.end()) {
            srv = it->second.srv.Get();
        }
    }

    // Use dummy texture if null
    if (!srv) {
        srv = m_state->dummyTextureSRV.Get();
    }

    // Bind to pixel shader (most common use case)
    m_psTextures[binding] = srv;
    m_context->PSSetShaderResources(binding, 1, &srv);

    // Also bind to vertex shader for vertex texture fetch
    m_vsTextures[binding] = srv;
    m_context->VSSetShaderResources(binding, 1, &srv);

    // Also bind to geometry shader (constant buffers are already GS-bound;
    // custom .lsh geometry stages may sample textures too)
    m_context->GSSetShaderResources(binding, 1, &srv);
}

void GfxCommandListDX11::bindSampler(SamplerHandle sampler, uint32_t binding, uint32_t) {
    if (binding >= MAX_SHADER_RESOURCES) {
        return;
    }

    ID3D11SamplerState* samplerState = nullptr;

    if (sampler.isValid()) {
        auto it = m_state->samplers.find(sampler.id);
        if (it != m_state->samplers.end()) {
            samplerState = it->second.sampler.Get();
        }
    }

    // Use dummy sampler if null
    if (!samplerState) {
        samplerState = m_state->dummySampler.Get();
    }

    // Bind to pixel shader
    m_psSamplers[binding] = samplerState;
    m_context->PSSetSamplers(binding, 1, &samplerState);

    // Also bind to vertex shader
    m_vsSamplers[binding] = samplerState;
    m_context->VSSetSamplers(binding, 1, &samplerState);

    // Also bind to geometry shader to match the SRV binding above
    m_context->GSSetSamplers(binding, 1, &samplerState);
}

void GfxCommandListDX11::pushConstants(ShaderStage, const void* data, uint32_t size, uint32_t offset) {
    // DirectX 11 doesn't have push constants like Vulkan
    // We emulate this by updating the material constant buffer
    if (offset + size > MATERIAL_CB_SIZE) {
        LOG_ERROR(LogCategory::Render, "Push constants exceed material buffer size");
        return;
    }

    std::memcpy(m_materialData.data() + offset, data, size);
    m_materialDataDirty = true;

    // Update the next uniform offset to be after the push constants data
    // This ensures setUniform* calls don't overwrite push constants
    uint32_t endOffset = offset + size;
    if (endOffset > m_nextUniformOffset) {
        m_nextUniformOffset = endOffset;
    }
}

void GfxCommandListDX11::setUniformFloat(const char* name, float value) {
    uint32_t offset = getUniformLocation(name, sizeof(float));
    if (offset + sizeof(float) <= MATERIAL_CB_SIZE) {
        std::memcpy(m_materialData.data() + offset, &value, sizeof(float));
        m_materialDataDirty = true;
    }
}

void GfxCommandListDX11::setUniformInt(const char* name, int value) {
    uint32_t offset = getUniformLocation(name, sizeof(int));
    if (offset + sizeof(int) <= MATERIAL_CB_SIZE) {
        std::memcpy(m_materialData.data() + offset, &value, sizeof(int));
        m_materialDataDirty = true;
    }
}

void GfxCommandListDX11::setUniformBool(const char* name, bool value) {
    // In HLSL, bool is typically passed as int (4 bytes)
    int intValue = value ? 1 : 0;
    uint32_t offset = getUniformLocation(name, sizeof(int));
    if (offset + sizeof(int) <= MATERIAL_CB_SIZE) {
        std::memcpy(m_materialData.data() + offset, &intValue, sizeof(int));
        m_materialDataDirty = true;
    }
}

void GfxCommandListDX11::setUniformVec2(const char* name, const Vec2& value) {
    uint32_t offset = getUniformLocation(name, sizeof(Vec2));
    if (offset + sizeof(Vec2) <= MATERIAL_CB_SIZE) {
        std::memcpy(m_materialData.data() + offset, &value, sizeof(Vec2));
        m_materialDataDirty = true;
    }
}

void GfxCommandListDX11::setUniformVec3(const char* name, const Vec3& value) {
    // Vec3 needs to be aligned to 16 bytes in HLSL (treated as float4)
    uint32_t offset = getUniformLocation(name, 16);
    if (offset + sizeof(Vec3) <= MATERIAL_CB_SIZE) {
        std::memcpy(m_materialData.data() + offset, &value, sizeof(Vec3));
        m_materialDataDirty = true;
    }
}

void GfxCommandListDX11::setUniformVec4(const char* name, const Vec4& value) {
    uint32_t offset = getUniformLocation(name, sizeof(Vec4));
    if (offset + sizeof(Vec4) <= MATERIAL_CB_SIZE) {
        std::memcpy(m_materialData.data() + offset, &value, sizeof(Vec4));
        m_materialDataDirty = true;
    }
}

void GfxCommandListDX11::setUniformMat4(const char* name, const math::Mat4& value) {
    uint32_t offset = getUniformLocation(name, sizeof(math::Mat4));
    if (offset + sizeof(math::Mat4) <= MATERIAL_CB_SIZE) {
        std::memcpy(m_materialData.data() + offset, &value, sizeof(math::Mat4));
        m_materialDataDirty = true;
    }
}

void GfxCommandListDX11::setUniformMat4Array(const char* name, const math::Mat4* values, size_t count) {
    // Check if this is bone transform data - use dedicated bone constant buffer
    if (std::strcmp(name, "u_BoneTransforms") == 0) {
        // Clamp to max bones
        size_t boneCount = std::min(count, static_cast<size_t>(MAX_BONES));
        uint32_t totalSize = static_cast<uint32_t>(sizeof(math::Mat4) * boneCount);

        if (totalSize <= BONE_CB_SIZE) {
            std::memcpy(m_boneData.data(), values, totalSize);
            m_boneDataDirty = true;
        }
        return;
    }

    // Regular mat4 array - use material constant buffer
    uint32_t totalSize = static_cast<uint32_t>(sizeof(math::Mat4) * count);
    uint32_t offset = getUniformLocation(name, totalSize);
    if (offset + totalSize <= MATERIAL_CB_SIZE) {
        std::memcpy(m_materialData.data() + offset, values, totalSize);
        m_materialDataDirty = true;
    }
}

void GfxCommandListDX11::setUniformColor(const char* name, const Color& value) {
    // Color is float4 (RGBA)
    uint32_t offset = getUniformLocation(name, sizeof(Color));
    if (offset + sizeof(Color) <= MATERIAL_CB_SIZE) {
        std::memcpy(m_materialData.data() + offset, &value, sizeof(Color));
        m_materialDataDirty = true;
    }
}

void GfxCommandListDX11::resetMaterialData() {
    std::memset(m_materialData.data(), 0, MATERIAL_CB_SIZE);
    std::memset(m_boneData.data(), 0, BONE_CB_SIZE);
    m_uniformLocationCache.clear();
    m_nextUniformOffset = 0;
    m_materialDataDirty = true;
    m_boneDataDirty = false;
}

void GfxCommandListDX11::draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) {
    applyPendingClears();
    updateMaterialConstantBuffer();
    updateBoneConstantBuffer();

    if (instanceCount > 1) {
        m_context->DrawInstanced(vertexCount, instanceCount, firstVertex, firstInstance);
    } else {
        m_context->Draw(vertexCount, firstVertex);
    }
}

void GfxCommandListDX11::drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) {
    applyPendingClears();
    updateMaterialConstantBuffer();
    updateBoneConstantBuffer();

    // Debug: Verify render state
    static int drawCallCount = 0;
    if (drawCallCount < 5) {
        
        drawCallCount++;
    }

    if (instanceCount > 1) {
        m_context->DrawIndexedInstanced(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
    } else {
        m_context->DrawIndexed(indexCount, firstIndex, vertexOffset);
    }
}

void GfxCommandListDX11::barrier() {
    // DirectX 11 handles resource barriers automatically
    // No explicit barrier needed for most cases
    m_context->Flush();
}

void GfxCommandListDX11::beginDebugMarker(const char* name) {
    if (!m_annotation && m_context) {
        m_context->QueryInterface(IID_PPV_ARGS(&m_annotation));
    }
    if (!m_annotation) {
        return;
    }

    const char* label = name ? name : "";
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, label, -1, nullptr, 0);
    if (wideLen <= 0) {
        m_annotation->BeginEvent(L"");
        return;
    }

    std::vector<wchar_t> wideLabel(static_cast<size_t>(wideLen));
    MultiByteToWideChar(CP_UTF8, 0, label, -1, wideLabel.data(), wideLen);
    m_annotation->BeginEvent(wideLabel.data());
}

void GfxCommandListDX11::endDebugMarker() {
    if (m_annotation) {
        m_annotation->EndEvent();
    }
}

void GfxCommandListDX11::beginRendering(DX11RenderTarget* target) {
    if (!target) {
        return;
    }

    ID3D11RenderTargetView* rtv = target->rtv.Get();
    ID3D11DepthStencilView* dsv = target->dsv.Get();

    m_context->OMSetRenderTargets(rtv ? 1 : 0, rtv ? &rtv : nullptr, dsv);

    // Apply pending clears
    applyPendingClears();
}

void GfxCommandListDX11::endRendering() {
    // DirectX 11 doesn't require explicit end rendering
    // State is maintained until changed
}

void GfxCommandListDX11::updateMaterialConstantBuffer() {
    if (!m_materialDataDirty || !m_materialConstantBuffer) {
        return;
    }

    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = m_context->Map(m_materialConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (SUCCEEDED(hr)) {
        std::memcpy(mapped.pData, m_materialData.data(), MATERIAL_CB_SIZE);
        m_context->Unmap(m_materialConstantBuffer.Get(), 0);

        // Bind to slot 0 of all shader stages
        ID3D11Buffer* cb = m_materialConstantBuffer.Get();
        m_context->VSSetConstantBuffers(0, 1, &cb);
        m_context->PSSetConstantBuffers(0, 1, &cb);
        m_context->GSSetConstantBuffers(0, 1, &cb);
        m_context->HSSetConstantBuffers(0, 1, &cb);
        m_context->DSSetConstantBuffers(0, 1, &cb);
    }

    m_materialDataDirty = false;
}

void GfxCommandListDX11::updateBoneConstantBuffer() {
    if (!m_boneDataDirty || !m_boneConstantBuffer) {
        return;
    }

    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = m_context->Map(m_boneConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (SUCCEEDED(hr)) {
        std::memcpy(mapped.pData, m_boneData.data(), BONE_CB_SIZE);
        m_context->Unmap(m_boneConstantBuffer.Get(), 0);

        // Bind to slot 2 of vertex shader (BoneData cbuffer is at register b2)
        ID3D11Buffer* cb = m_boneConstantBuffer.Get();
        m_context->VSSetConstantBuffers(2, 1, &cb);
    }

    m_boneDataDirty = false;
}

void GfxCommandListDX11::applyPendingClears() {
    if (!m_currentRenderTarget) {
        return;
    }

    if (m_pendingColorClear && m_currentRenderTarget->rtv) {
        float clearColorArray[4] = {m_clearColor.r, m_clearColor.g, m_clearColor.b, m_clearColor.a};
        m_context->ClearRenderTargetView(m_currentRenderTarget->rtv.Get(), clearColorArray);
        m_pendingColorClear = false;
    }

    if ((m_pendingDepthClear || m_pendingStencilClear) && m_currentRenderTarget->dsv) {
        UINT clearFlags = 0;
        if (m_pendingDepthClear) clearFlags |= D3D11_CLEAR_DEPTH;
        if (m_pendingStencilClear) clearFlags |= D3D11_CLEAR_STENCIL;

        m_context->ClearDepthStencilView(m_currentRenderTarget->dsv.Get(), clearFlags, m_clearDepth, m_clearStencil);
        m_pendingDepthClear = false;
        m_pendingStencilClear = false;
    }
}

uint32_t GfxCommandListDX11::getUniformLocation(const char* name, uint32_t size) {
    // First check our local cache
    auto cacheIt = m_uniformLocationCache.find(name);
    if (cacheIt != m_uniformLocationCache.end()) {
        return cacheIt->second;
    }

    // Try to look up the offset from the pipeline's shader reflection data
    // This gives us the exact offset that matches the HLSL cbuffer layout
    if (m_state->currentPipeline) {
        auto reflIt = m_state->currentPipeline->uniformOffsets.find(name);
        if (reflIt != m_state->currentPipeline->uniformOffsets.end()) {
            // Found the offset from shader reflection - use it and cache it
            uint32_t offset = reflIt->second;
            m_uniformLocationCache[name] = offset;

            // Update next offset to be after this variable (for any dynamic allocations)
            uint32_t endOffset = offset + size;
            if (endOffset > m_nextUniformOffset) {
                m_nextUniformOffset = endOffset;
            }

            return offset;
        }
    }

    // Fallback: dynamically allocate offset (for uniforms not in reflection data)
    // Align to 16 bytes (DirectX constant buffer alignment requirement)
    uint32_t alignedOffset = (m_nextUniformOffset + 15) & ~15;

    // Check if we have enough space
    if (alignedOffset + size > MATERIAL_CB_SIZE) {
        LOG_ERROR(LogCategory::Render, "Material constant buffer overflow for uniform: {}", name);
        return 0;
    }

    // Cache the location
    m_uniformLocationCache[name] = alignedOffset;
    m_nextUniformOffset = alignedOffset + size;

    return alignedOffset;
}

} // namespace lupine

#endif // LUPINE_HAS_DIRECTX11
