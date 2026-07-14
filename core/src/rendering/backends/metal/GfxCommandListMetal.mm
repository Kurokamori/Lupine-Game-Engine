/**
 * Metal Command List Implementation
 *
 * This file implements command recording for the Metal backend.
 * Uses Objective-C++ (.mm) for Metal API calls.
 */

#include "lupine/rendering/backends/metal/GfxCommandListMetal.hpp"
#include "lupine/rendering/backends/GfxDeviceMetal.hpp"
#include "lupine/logger/Logger.hpp"

#ifdef LUPINE_HAS_METAL
#ifdef __APPLE__

#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

namespace lupine {

// ============================================================================
// Constructor / Destructor
// ============================================================================

GfxCommandListMetal::GfxCommandListMetal(GfxDeviceMetal* device, MetalState* state)
    : m_device(device)
    , m_state(state)
{
    // Allocate material data buffer
    m_materialData.resize(MATERIAL_CB_SIZE, 0);
    m_boneData.resize(BONE_CB_SIZE, 0);
    m_pushConstantsData.resize(PUSH_CONSTANTS_MAX_SIZE, 0);

    // Create material constant buffer
    m_materialBuffer = [state->device newBufferWithLength:MATERIAL_CB_SIZE
                                                  options:MTLResourceStorageModeShared];

    // Create bone constant buffer
    m_boneBuffer = [state->device newBufferWithLength:BONE_CB_SIZE
                                              options:MTLResourceStorageModeShared];

    // Initialize vertex buffer bindings (support up to 8 bindings)
    m_vertexBuffers.resize(8);
}

GfxCommandListMetal::~GfxCommandListMetal() {
    endComputeEncoder();
    endEncoder();
    m_materialBuffer = nil;
    m_boneBuffer = nil;
    m_commandBuffer = nil;
}

// ============================================================================
// Internal Methods
// ============================================================================

void GfxCommandListMetal::beginRendering(MetalRenderTarget* target) {
    m_currentRenderTarget = target;

    // Create a new command buffer
    m_commandBuffer = [m_state->commandQueue commandBuffer];
    if (!m_commandBuffer) {
        LOG_ERROR(LogCategory::Render, "Metal: Failed to create command buffer");
        return;
    }

    // Don't set pending clears by default - let clearColor/clearDepth set them
    // This allows subsequent cameras to NOT clear and preserve previous camera output
    m_pendingColorClear = false;
    m_pendingDepthClear = false;
    m_pendingStencilClear = false;

    // Reset viewport and scissor flags so defaults are used unless explicitly set
    m_hasViewport = false;
    m_hasScissor = false;
}

void GfxCommandListMetal::endRendering() {
    endComputeEncoder();
    endEncoder();
}

void GfxCommandListMetal::ensureEncoder() {
    if (m_encoderActive) return;
    if (!m_currentRenderTarget || !m_commandBuffer) {
        LOG_ERROR(LogCategory::Render, "Metal: ensureEncoder failed - renderTarget={}, cmdBuffer={}",
                  m_currentRenderTarget != nullptr, m_commandBuffer != nil);
        return;
    }

    // End compute encoder if active (can't have both at once)
    if (m_computeEncoderActive) {
        endComputeEncoder();
    }

    // Create render pass descriptor
    MTLRenderPassDescriptor* rpDesc = [MTLRenderPassDescriptor renderPassDescriptor];

    // Color attachment
    if (m_currentRenderTarget->hasColor && m_currentRenderTarget->colorTexture) {
        id<MTLTexture> colorTex = m_currentRenderTarget->colorTexture;

        // DEBUG: Log the color texture being used
        LOG_DEBUG(LogCategory::Render, "Metal: ensureEncoder colorTex={}, format={}, size={}x{}",
                  (__bridge void*)colorTex, (int)[colorTex pixelFormat],
                  (int)[colorTex width], (int)[colorTex height]);

        // Handle cube map face selection
        if (m_currentRenderTarget->isCubeMap && m_currentRenderTarget->currentCubeFace >= 0) {
            rpDesc.colorAttachments[0].texture = colorTex;
            rpDesc.colorAttachments[0].slice = m_currentRenderTarget->currentCubeFace;
        } else {
            rpDesc.colorAttachments[0].texture = colorTex;
        }

        rpDesc.colorAttachments[0].loadAction = m_pendingColorClear ? MTLLoadActionClear : MTLLoadActionLoad;
        rpDesc.colorAttachments[0].storeAction = MTLStoreActionStore;
        rpDesc.colorAttachments[0].clearColor = m_clearColor;
        m_pendingColorClear = false;
    }

    // Depth attachment
    if (m_currentRenderTarget->hasDepth && m_currentRenderTarget->depthTexture) {
        id<MTLTexture> depthTex = m_currentRenderTarget->depthTexture;

        // Handle cube map face selection
        if (m_currentRenderTarget->isCubeMap && m_currentRenderTarget->currentCubeFace >= 0) {
            rpDesc.depthAttachment.texture = depthTex;
            rpDesc.depthAttachment.slice = m_currentRenderTarget->currentCubeFace;
        } else {
            rpDesc.depthAttachment.texture = depthTex;
        }

        rpDesc.depthAttachment.loadAction = m_pendingDepthClear ? MTLLoadActionClear : MTLLoadActionLoad;
        rpDesc.depthAttachment.storeAction = m_currentRenderTarget->depthStoreAction;
        rpDesc.depthAttachment.clearDepth = m_clearDepth;
        m_pendingDepthClear = false;

        // Stencil attachment (if present)
        if (m_currentRenderTarget->hasStencil) {
            rpDesc.stencilAttachment.texture = depthTex;
            rpDesc.stencilAttachment.loadAction = m_pendingStencilClear ? MTLLoadActionClear : MTLLoadActionLoad;
            rpDesc.stencilAttachment.storeAction = m_currentRenderTarget->depthStoreAction;
            rpDesc.stencilAttachment.clearStencil = m_clearStencil;
            m_pendingStencilClear = false;
        }
    }

    // Create render command encoder
    m_encoder = [m_commandBuffer renderCommandEncoderWithDescriptor:rpDesc];
    if (!m_encoder) {
        LOG_ERROR(LogCategory::Render, "Metal: Failed to create render command encoder");
        return;
    }

    m_encoderActive = true;
    m_pipelineBoundToEncoder = false;  // Reset - pipeline needs to be re-bound to new encoder
    LOG_DEBUG(LogCategory::Render, "Metal: Created render encoder for target {}x{}, hasColor={}, hasDepth={}",
             m_currentRenderTarget->width, m_currentRenderTarget->height,
             m_currentRenderTarget->hasColor, m_currentRenderTarget->hasDepth);

    // Apply pending viewport and scissor, or use defaults based on render target size
    MTLViewport appliedViewport = {0, 0, 1, 1, 0, 1};
    MTLScissorRect appliedScissor = {0, 0, 1, 1};

    if (m_hasViewport) {
        appliedViewport = m_pendingViewport;
    } else if (m_currentRenderTarget) {
        appliedViewport.originX = 0;
        appliedViewport.originY = 0;
        appliedViewport.width = m_currentRenderTarget->width;
        appliedViewport.height = m_currentRenderTarget->height;
        appliedViewport.znear = 0.0;
        appliedViewport.zfar = 1.0;
    }
    [m_encoder setViewport:appliedViewport];

    if (m_hasScissor) {
        appliedScissor = m_pendingScissor;
    } else if (m_currentRenderTarget) {
        appliedScissor.x = 0;
        appliedScissor.y = 0;
        appliedScissor.width = m_currentRenderTarget->width;
        appliedScissor.height = m_currentRenderTarget->height;
    }
    [m_encoder setScissorRect:appliedScissor];

    LOG_DEBUG(LogCategory::Render, "Metal: ensureEncoder viewport=({},{} {}x{} z:{}-{}), scissor=({},{} {}x{})",
              appliedViewport.originX, appliedViewport.originY,
              appliedViewport.width, appliedViewport.height,
              appliedViewport.znear, appliedViewport.zfar,
              appliedScissor.x, appliedScissor.y,
              appliedScissor.width, appliedScissor.height);
}

void GfxCommandListMetal::endEncoder() {
    if (m_encoderActive && m_encoder) {
        [m_encoder endEncoding];
        m_encoder = nil;
        m_encoderActive = false;
    }
}

uint32_t GfxCommandListMetal::getUniformLocation(const char* name, uint32_t size) {
    auto it = m_uniformLocationCache.find(name);
    if (it != m_uniformLocationCache.end()) {
        return it->second;
    }

    // Use fixed offsets for common uniform names to match Metal shader struct layouts
    // Base MaterialUniforms struct (shared by most shaders):
    // struct MaterialUniforms {
    //     float4x4 u_ViewProjection;   // offset 0   (64 bytes)
    //     float4x4 u_Model;            // offset 64  (64 bytes)
    //     float4x4 u_NormalMatrix;     // offset 128 (64 bytes)
    //     float4 u_TintColor;          // offset 192 (16 bytes)
    //     int u_UseTexture;            // offset 208 (4 bytes)
    //     float u_AlphaCutoff;         // offset 212 (4 bytes)
    //     float _pad1;                 // offset 216 (4 bytes)
    //     float _pad2;                 // offset 220 (4 bytes)
    //     float4 u_UVRect;             // offset 224 (16 bytes)
    //     // PBR extensions:
    //     float4 u_TextureFlags;       // offset 240 (16 bytes)
    //     float4 u_MaterialParams1;    // offset 256 (16 bytes)
    //     float4 u_MaterialParams2;    // offset 272 (16 bytes)
    //     float4 u_CameraPosition;     // offset 288 (16 bytes)
    //     float4 u_AlbedoColor;        // offset 304 (16 bytes)
    // };
    // Total: 320 bytes for PBR, 240 bytes for basic shaders
    //
    // Additional uniforms start at offset 320+

    std::string nameStr(name);
    uint32_t offset = 0;

    // Fixed offsets for standard uniform names (matching shader struct order)
    if (nameStr == "u_ViewProjection") {
        offset = 0;
    } else if (nameStr == "u_Model") {
        offset = 64;
    } else if (nameStr == "u_NormalMatrix") {
        offset = 128;
    } else if (nameStr == "u_TintColor") {
        offset = 192;
    } else if (nameStr == "u_UseTexture") {
        offset = 208;
    } else if (nameStr == "u_AlphaCutoff") {
        offset = 212;
    } else if (nameStr == "u_UVRect") {
        offset = 224;
    } else if (nameStr == "u_TextureFlags") {
        // PBR texture flags: x=useAlbedo, y=useMetallicRoughness, z=useNormal, w=useEmissive
        offset = 240;
    } else if (nameStr == "u_MaterialParams1") {
        // PBR params: x=metallic, y=roughness, z=normalScale, w=emissiveStrength
        offset = 256;
    } else if (nameStr == "u_MaterialParams2") {
        // PBR params: x=alphaCutoff, y=aoStrength, z=heightScale, w=unused
        offset = 272;
    } else if (nameStr == "u_CameraPosition" || nameStr == "u_ViewPos") {
        offset = 288;
    } else if (nameStr == "u_AlbedoColor") {
        // PBR base material color (separate from tint)
        offset = 304;
    } else if (nameStr == "u_UseSkinning") {
        // Skeletal animation flag - at offset 320 (after PerObjectUniforms, before extended uniforms)
        offset = 320;
    } else if (nameStr == "u_ModelViewProjection") {
        // Some shaders use MVP instead - place at 324
        offset = 324;
    } else if (nameStr == "u_View") {
        offset = 384;
    } else if (nameStr == "u_Projection") {
        offset = 448;
    } else if (nameStr == "u_Time") {
        offset = 512;
    } else if (nameStr == "u_Color" || nameStr == "u_BaseColor") {
        offset = 544;
    } else if (nameStr == "u_AmbientColor") {
        offset = 560;
    } else if (nameStr == "u_LightDirection") {
        offset = 576;
    } else if (nameStr == "u_LightColor") {
        offset = 592;
    } else if (nameStr == "u_Metallic") {
        // Legacy individual PBR params (for non-vec4 usage)
        offset = 608;
    } else if (nameStr == "u_Roughness") {
        offset = 612;
    } else if (nameStr == "u_NormalScale") {
        offset = 616;
    } else if (nameStr == "u_EmissiveStrength") {
        offset = 620;
    }
    // Skybox-specific uniforms (fixed offsets starting at 624)
    else if (nameStr == "u_SkyboxType") {
        offset = 624;
    } else if (nameStr == "u_SkyboxColor") {
        offset = 640;  // 16-byte aligned for float4
    } else if (nameStr == "u_SkyTopColor") {
        offset = 656;
    } else if (nameStr == "u_SkyHorizonColor") {
        offset = 672;
    } else if (nameStr == "u_SkyBottomColor") {
        offset = 688;
    } else {
        // For unknown uniforms, use dynamic allocation starting at 704 (after known uniforms)
        uint32_t alignedOffset = std::max(704u, (m_nextUniformOffset + 15) & ~15);
        m_uniformLocationCache[name] = alignedOffset;
        m_nextUniformOffset = alignedOffset + size;
        return alignedOffset;
    }

    m_uniformLocationCache[name] = offset;
    return offset;
}

void GfxCommandListMetal::updateMaterialBuffer() {
    if (!m_materialDataDirty) return;

    // Only copy the portion of the buffer that's actually been used
    // This avoids copying 64KB every draw call
    size_t copySize = std::min(m_materialData.size(), static_cast<size_t>(m_nextUniformOffset > 0 ? ((m_nextUniformOffset + 255) & ~255) : 512));
    memcpy(m_materialBuffer.contents, m_materialData.data(), copySize);

    // For shared storage mode, we need to tell Metal the data has changed
    // This is done by calling didModifyRange on macOS
#if TARGET_OS_OSX
    [m_materialBuffer didModifyRange:NSMakeRange(0, copySize)];
#endif

    m_materialDataDirty = false;
}

void GfxCommandListMetal::updateBoneBuffer() {
    if (!m_boneDataDirty) return;

    memcpy(m_boneBuffer.contents, m_boneData.data(), m_boneData.size());
    m_boneDataDirty = false;
}

void GfxCommandListMetal::applyPendingClears() {
    // In Metal, clears are applied when creating the render pass encoder via loadAction
    // If the encoder is already active, the clears are stored and will be used
    // when the encoder is next created (after setRenderTarget or similar)
    //
    // We do NOT recreate the encoder here because:
    // 1. It would discard all previous draw calls in this render pass
    // 2. Clears are typically set at the start before any drawing
    // 3. If clears are set after drawing has started, they should be ignored
    //    (the caller should call setRenderTarget to start a new render pass)
    //
    // The pending clear flags will be used by ensureEncoder() when it creates
    // a new encoder (e.g., after setRenderTarget ends the current one)
}

// ============================================================================
// IGfxCommandList Interface
// ============================================================================

void GfxCommandListMetal::setRenderTarget(RenderTargetHandle target) {
    if (m_currentRenderTargetHandle.id == target.id) return;

    auto it = m_state->renderTargets.find(target.id);
    if (it == m_state->renderTargets.end()) {
        LOG_WARN(LogCategory::Render, "Metal: Invalid render target handle");
        return;
    }

    // End current encoder if active
    endEncoder();

    m_currentRenderTargetHandle = target;
    m_currentRenderTarget = &it->second;

    // Encoder will be created lazily when needed
}

void GfxCommandListMetal::setViewport(const Viewport& viewport) {
    // Store viewport for later - will be applied when encoder is created or if already exists
    m_pendingViewport.originX = viewport.x;
    m_pendingViewport.originY = viewport.y;
    m_pendingViewport.width = viewport.width;
    m_pendingViewport.height = viewport.height;
    m_pendingViewport.znear = viewport.minDepth;
    m_pendingViewport.zfar = viewport.maxDepth;
    m_hasViewport = true;

    // If encoder already exists, apply immediately
    if (m_encoderActive && m_encoder) {
        [m_encoder setViewport:m_pendingViewport];
    }
}

void GfxCommandListMetal::setScissor(const ScissorRect& scissor) {
    // Treat 0x0 scissor as "disable scissor" - use full render target size
    // Other backends may ignore scissor when set to 0x0, but Metal clips everything
    if (scissor.width == 0 || scissor.height == 0) {
        // Reset to default (full render target) - will be applied in ensureEncoder
        m_hasScissor = false;
        return;
    }

    // Store scissor for later - will be applied when encoder is created or if already exists
    m_pendingScissor.x = scissor.x;
    m_pendingScissor.y = scissor.y;
    m_pendingScissor.width = scissor.width;
    m_pendingScissor.height = scissor.height;
    m_hasScissor = true;

    // If encoder already exists, apply immediately
    if (m_encoderActive && m_encoder) {
        [m_encoder setScissorRect:m_pendingScissor];
    }
}

void GfxCommandListMetal::clearColor(const Color& color) {
    m_clearColor = MTLClearColorMake(color.r, color.g, color.b, color.a);
    m_pendingColorClear = true;
    // Don't call applyPendingClears - clears will be applied when encoder is created
    // If encoder already exists, we can't change the load action, but the clear color
    // is already applied during encoder creation
}

void GfxCommandListMetal::clearDepth(float depth) {
    m_clearDepth = depth;
    m_pendingDepthClear = true;
    // Don't call applyPendingClears - clears will be applied when encoder is created
}

void GfxCommandListMetal::clearStencil(uint8_t stencil) {
    m_clearStencil = stencil;
    m_pendingStencilClear = true;
    // Don't call applyPendingClears - clears will be applied when encoder is created
}

void GfxCommandListMetal::bindPipeline(PipelineHandle pipeline) {
    auto it = m_state->pipelines.find(pipeline.id);
    if (it == m_state->pipelines.end()) {
        LOG_WARN(LogCategory::Render, "Metal: Invalid pipeline handle");
        return;
    }

    ensureEncoder();
    if (!m_encoder) return;

    // Only skip if same pipeline AND already bound to this encoder
    // m_pipelineBoundToEncoder is reset when encoder is recreated
    if (m_currentPipeline.id == pipeline.id && m_pipelineBoundToEncoder) return;

    const MetalPipeline& p = it->second;
    m_currentPipeline = pipeline;
    m_pipelineBoundToEncoder = true;

    // Set render pipeline state
    [m_encoder setRenderPipelineState:p.pipelineState];

    // Set depth-stencil state
    // DEBUG: Log if depthStencilState is nil
    if (!p.depthStencilState) {
        LOG_WARN(LogCategory::Render, "Metal: bindPipeline - depthStencilState is nil for pipeline {}", pipeline.id);
    }
    [m_encoder setDepthStencilState:p.depthStencilState];

    // Set rasterizer state
    [m_encoder setCullMode:p.cullMode];
    [m_encoder setFrontFacingWinding:p.frontFace];
    [m_encoder setTriangleFillMode:p.fillMode];

    LOG_DEBUG(LogCategory::Render, "Metal: bindPipeline {} - cullMode={}, frontFace={}, fillMode={}",
              pipeline.id, (int)p.cullMode, (int)p.frontFace, (int)p.fillMode);

    // Set depth bias
    if (p.depthBiasEnable) {
        [m_encoder setDepthBias:p.depthBiasConstant slopeScale:p.depthBiasSlope clamp:p.depthBiasClamp];
    } else {
        [m_encoder setDepthBias:0 slopeScale:0 clamp:0];
    }
}

void GfxCommandListMetal::setLineWidth(float width) {
    // Metal doesn't support setting line width directly
    // Line width is always 1.0 in Metal
    (void)width;
}

void GfxCommandListMetal::bindVertexBuffer(BufferHandle buffer, uint32_t binding, uint64_t offset) {
    auto it = m_state->buffers.find(buffer.id);
    if (it == m_state->buffers.end()) {
        LOG_WARN(LogCategory::Render, "Metal: Invalid vertex buffer handle");
        return;
    }

    ensureEncoder();
    if (!m_encoder) return;

    // Metal uses buffer index 0 for vertex data by convention
    // Higher indices are used for other vertex streams or constant buffers
    [m_encoder setVertexBuffer:it->second.buffer offset:offset atIndex:binding];

    if (binding < m_vertexBuffers.size()) {
        m_vertexBuffers[binding].buffer = buffer;
        m_vertexBuffers[binding].offset = offset;
    }
}

void GfxCommandListMetal::bindIndexBuffer(BufferHandle buffer, IndexFormat format, uint64_t offset) {
    auto it = m_state->buffers.find(buffer.id);
    if (it == m_state->buffers.end()) {
        LOG_WARN(LogCategory::Render, "Metal: Invalid index buffer handle");
        return;
    }

    m_currentIndexBuffer = buffer;
    m_currentIndexFormat = format;
    m_indexBufferOffset = offset;
}

void GfxCommandListMetal::bindUniformBuffer(UniformBufferHandle buffer, uint32_t binding, uint32_t set) {
    auto it = m_state->uniformBuffers.find(buffer.id);
    if (it == m_state->uniformBuffers.end()) {
        LOG_WARN(LogCategory::Render, "Metal: Invalid uniform buffer handle");
        return;
    }

    ensureEncoder();
    if (!m_encoder) return;

    // In Metal, we bind buffers to both vertex and fragment stages
    // Use set as an offset for the buffer index (binding + set * 16)
    uint32_t bufferIndex = binding + set * 16;

    // Debug: Log light buffer binding (buffer index 3)
    if (bufferIndex == 3) {
        LOG_INFO(LogCategory::Render, "Metal: Binding LIGHT buffer id={} to fragment buffer index 3, size={}",
                 buffer.id, it->second.size);
    }

    [m_encoder setVertexBuffer:it->second.buffer offset:0 atIndex:bufferIndex];
    [m_encoder setFragmentBuffer:it->second.buffer offset:0 atIndex:bufferIndex];
}

void GfxCommandListMetal::bindTexture(TextureHandle texture, uint32_t binding, uint32_t set) {
    id<MTLTexture> mtlTexture = m_state->dummyTexture;
    bool foundTexture = false;

    auto it = m_state->textures.find(texture.id);
    if (it != m_state->textures.end()) {
        mtlTexture = it->second.texture;
        foundTexture = true;
    }

    ensureEncoder();
    if (!m_encoder) {
        LOG_WARN(LogCategory::Render, "Metal: bindTexture failed - no encoder, texId={}, binding={}", texture.id, binding);
        return;
    }

    // Log texture binding for debugging
    if (foundTexture) {
        LOG_DEBUG(LogCategory::Render, "Metal: bindTexture id={} to binding={}, mtlTex={}, size={}x{}, format={}",
                  texture.id, binding, (__bridge void*)mtlTexture,
                  (int)[mtlTexture width], (int)[mtlTexture height], (int)[mtlTexture pixelFormat]);
    } else {
        LOG_WARN(LogCategory::Render, "Metal: bindTexture id={} NOT FOUND, using dummy texture at binding={}", texture.id, binding);
    }

    // Bind to fragment stage (most common use case)
    // Also bind to vertex stage for vertex texture fetch
    [m_encoder setFragmentTexture:mtlTexture atIndex:binding];
    [m_encoder setVertexTexture:mtlTexture atIndex:binding];
}

void GfxCommandListMetal::bindSampler(SamplerHandle sampler, uint32_t binding, uint32_t set) {
    id<MTLSamplerState> mtlSampler = m_state->dummySampler;
    bool foundSampler = false;

    auto it = m_state->samplers.find(sampler.id);
    if (it != m_state->samplers.end()) {
        mtlSampler = it->second.sampler;
        foundSampler = true;
    }

    ensureEncoder();
    if (!m_encoder) {
        LOG_WARN(LogCategory::Render, "Metal: bindSampler failed - no encoder, samplerId={}, binding={}", sampler.id, binding);
        return;
    }

    LOG_DEBUG(LogCategory::Render, "Metal: bindSampler id={} to binding={}, found={}", sampler.id, binding, foundSampler);

    [m_encoder setFragmentSamplerState:mtlSampler atIndex:binding];
    [m_encoder setVertexSamplerState:mtlSampler atIndex:binding];
}

void GfxCommandListMetal::pushConstants(ShaderStage stage, const void* data, uint32_t size, uint32_t offset) {
    ensureEncoder();
    if (!m_encoder || !data) return;

    // In Metal, push constants are implemented via setBytes or buffer updates
    // For small data (< 4KB), setBytes is preferred
    // Use buffer index 16 to match shader's [[buffer(16)]] binding for MaterialUniforms
    //
    // IMPORTANT: Always set both vertex AND fragment bytes because the uniform struct
    // is shared between vertex and fragment shaders in Metal. The vertex shader may use
    // matrices while the fragment shader uses color/texture flags from the same struct.
    if (size <= 4096) {
        // Log the push constants data for debugging
        const float* floatData = reinterpret_cast<const float*>(data);
        const int32_t* intData = reinterpret_cast<const int32_t*>(data);
        LOG_DEBUG(LogCategory::Render, "Metal: pushConstants size={}, VP[0-3]: {}, {}, {}, {}",
                  size, floatData[0], floatData[1], floatData[2], floatData[3]);
        LOG_DEBUG(LogCategory::Render, "Metal: pushConstants Model[0-3]: {}, {}, {}, {}",
                  floatData[16], floatData[17], floatData[18], floatData[19]);
        LOG_DEBUG(LogCategory::Render, "Metal: pushConstants TintColor: {}, {}, {}, {}",
                  floatData[48], floatData[49], floatData[50], floatData[51]);
        // u_UseTexture is at offset 208 bytes = 52 floats = index 52
        LOG_DEBUG(LogCategory::Render, "Metal: pushConstants u_UseTexture={} (at offset 208)", intData[52]);

        // Store the push constants data so setUniform* can update it later
        // This is critical because applyPropertyOverrides sets uniforms AFTER pushConstants
        uint32_t copySize = std::min(size, PUSH_CONSTANTS_MAX_SIZE);
        memcpy(m_pushConstantsData.data(), data, copySize);

        // Always bind to both stages since uniform data is typically shared
        [m_encoder setVertexBytes:data length:size atIndex:16];
        [m_encoder setFragmentBytes:data length:size atIndex:16];

        // Mark that pushConstants was used so we don't overwrite with material buffer in draw calls
        m_pushConstantsUsed = true;
        m_pushConstantsDirty = false;  // Just bound, so not dirty yet

        // Also store the size so we know how much data was set
        m_pushConstantsSize = size;
    }
}

// ============================================================================
// Uniform Setting Methods
// ============================================================================

void GfxCommandListMetal::setUniformFloat(const char* name, float value) {
    uint32_t offset = getUniformLocation(name, sizeof(float));
    memcpy(m_materialData.data() + offset, &value, sizeof(float));
    m_materialDataDirty = true;

    // Also update push constants data if active (for uniforms set after pushConstants)
    if (m_pushConstantsUsed && offset < PUSH_CONSTANTS_MAX_SIZE) {
        memcpy(m_pushConstantsData.data() + offset, &value, sizeof(float));
        m_pushConstantsDirty = true;
    }
}

void GfxCommandListMetal::setUniformInt(const char* name, int value) {
    uint32_t offset = getUniformLocation(name, sizeof(int));
    memcpy(m_materialData.data() + offset, &value, sizeof(int));
    m_materialDataDirty = true;

    // Also update push constants data if active (for uniforms set after pushConstants)
    if (m_pushConstantsUsed && offset < PUSH_CONSTANTS_MAX_SIZE) {
        memcpy(m_pushConstantsData.data() + offset, &value, sizeof(int));
        m_pushConstantsDirty = true;
    }
}

void GfxCommandListMetal::setUniformBool(const char* name, bool value) {
    int intValue = value ? 1 : 0;
    uint32_t offset = getUniformLocation(name, sizeof(int));
    memcpy(m_materialData.data() + offset, &intValue, sizeof(int));
    m_materialDataDirty = true;

    // Also update push constants data if active (for uniforms set after pushConstants)
    // This is critical for u_UseTexture which is set via applyPropertyOverrides AFTER pushConstants
    if (m_pushConstantsUsed && offset < PUSH_CONSTANTS_MAX_SIZE) {
        memcpy(m_pushConstantsData.data() + offset, &intValue, sizeof(int));
        m_pushConstantsDirty = true;
    }
}

void GfxCommandListMetal::setUniformVec2(const char* name, const Vec2& value) {
    uint32_t offset = getUniformLocation(name, sizeof(Vec2));
    memcpy(m_materialData.data() + offset, &value, sizeof(Vec2));
    m_materialDataDirty = true;

    // Also update push constants data if active
    if (m_pushConstantsUsed && offset + sizeof(Vec2) <= PUSH_CONSTANTS_MAX_SIZE) {
        memcpy(m_pushConstantsData.data() + offset, &value, sizeof(Vec2));
        m_pushConstantsDirty = true;
    }
}

void GfxCommandListMetal::setUniformVec3(const char* name, const Vec3& value) {
    // Vec3 takes 16 bytes in Metal due to alignment
    uint32_t offset = getUniformLocation(name, 16);
    memcpy(m_materialData.data() + offset, &value, sizeof(Vec3));
    m_materialDataDirty = true;

    // Also update push constants data if active
    if (m_pushConstantsUsed && offset + sizeof(Vec3) <= PUSH_CONSTANTS_MAX_SIZE) {
        memcpy(m_pushConstantsData.data() + offset, &value, sizeof(Vec3));
        m_pushConstantsDirty = true;
    }
}

void GfxCommandListMetal::setUniformVec4(const char* name, const Vec4& value) {
    uint32_t offset = getUniformLocation(name, sizeof(Vec4));
    memcpy(m_materialData.data() + offset, &value, sizeof(Vec4));
    m_materialDataDirty = true;

    // Also update push constants data if active
    if (m_pushConstantsUsed && offset + sizeof(Vec4) <= PUSH_CONSTANTS_MAX_SIZE) {
        memcpy(m_pushConstantsData.data() + offset, &value, sizeof(Vec4));
        m_pushConstantsDirty = true;
    }
}

void GfxCommandListMetal::setUniformMat4(const char* name, const math::Mat4& value) {
    uint32_t offset = getUniformLocation(name, sizeof(math::Mat4));
    memcpy(m_materialData.data() + offset, &value, sizeof(math::Mat4));
    m_materialDataDirty = true;

    // Also update push constants data if active
    if (m_pushConstantsUsed && offset + sizeof(math::Mat4) <= PUSH_CONSTANTS_MAX_SIZE) {
        memcpy(m_pushConstantsData.data() + offset, &value, sizeof(math::Mat4));
        m_pushConstantsDirty = true;
    }
}

void GfxCommandListMetal::setUniformMat4Array(const char* name, const math::Mat4* values, size_t count) {
    // Check if this is bone data (u_BoneMatrices)
    std::string nameStr(name);
    if (nameStr == "u_BoneMatrices" || nameStr.find("Bone") != std::string::npos) {
        size_t copyCount = std::min(count, static_cast<size_t>(MAX_BONES));
        memcpy(m_boneData.data(), values, copyCount * sizeof(math::Mat4));
        m_boneDataDirty = true;
    } else {
        uint32_t offset = getUniformLocation(name, static_cast<uint32_t>(count * sizeof(math::Mat4)));
        memcpy(m_materialData.data() + offset, values, count * sizeof(math::Mat4));
        m_materialDataDirty = true;
    }
}

void GfxCommandListMetal::setUniformColor(const char* name, const Color& value) {
    uint32_t offset = getUniformLocation(name, sizeof(Color));
    memcpy(m_materialData.data() + offset, &value, sizeof(Color));
    m_materialDataDirty = true;

    // Also update push constants data if active
    if (m_pushConstantsUsed && offset + sizeof(Color) <= PUSH_CONSTANTS_MAX_SIZE) {
        memcpy(m_pushConstantsData.data() + offset, &value, sizeof(Color));
        m_pushConstantsDirty = true;
    }
}

void GfxCommandListMetal::resetMaterialData() {
    std::fill(m_materialData.begin(), m_materialData.end(), 0);
    m_uniformLocationCache.clear();
    m_nextUniformOffset = 0;
    m_materialDataDirty = true;
}

// ============================================================================
// Draw Commands
// ============================================================================

void GfxCommandListMetal::draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) {
    ensureEncoder();
    if (!m_encoder) {
        LOG_ERROR(LogCategory::Render, "Metal: draw called but no encoder available");
        return;
    }

    // Only bind material buffer if pushConstants wasn't used
    if (!m_pushConstantsUsed) {
        updateMaterialBuffer();
        [m_encoder setVertexBuffer:m_materialBuffer offset:0 atIndex:16];
        [m_encoder setFragmentBuffer:m_materialBuffer offset:0 atIndex:16];
    } else {
        // Re-bind push constants if uniforms were modified after pushConstants was called
        if (m_pushConstantsDirty && m_pushConstantsSize > 0) {
            [m_encoder setVertexBytes:m_pushConstantsData.data() length:m_pushConstantsSize atIndex:16];
            [m_encoder setFragmentBytes:m_pushConstantsData.data() length:m_pushConstantsSize atIndex:16];
            m_pushConstantsDirty = false;
        }
    }

    // Always update and bind bone buffer (slot 17)
    updateBoneBuffer();
    [m_encoder setVertexBuffer:m_boneBuffer offset:0 atIndex:17];

    // Reset flag after draw
    m_pushConstantsUsed = false;

    // Get primitive type from current pipeline
    MTLPrimitiveType primitiveType = MTLPrimitiveTypeTriangle;
    auto pipeIt = m_state->pipelines.find(m_currentPipeline.id);
    if (pipeIt != m_state->pipelines.end()) {
        primitiveType = pipeIt->second.mtlPrimitiveType;
    }

    LOG_INFO(LogCategory::Render, "Metal: draw (non-indexed) vertexCount={}, pipeline={}, primitiveType={}, encoder={}",
             vertexCount, m_currentPipeline.id, (int)primitiveType, (__bridge void*)m_encoder);

    [m_encoder drawPrimitives:primitiveType
                  vertexStart:firstVertex
                  vertexCount:vertexCount
                instanceCount:instanceCount
                 baseInstance:firstInstance];
}

void GfxCommandListMetal::drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) {
    ensureEncoder();
    if (!m_encoder) {
        LOG_ERROR(LogCategory::Render, "Metal: drawIndexed called but no encoder available");
        return;
    }

    // Get index buffer
    auto bufIt = m_state->buffers.find(m_currentIndexBuffer.id);
    if (bufIt == m_state->buffers.end()) {
        LOG_WARN(LogCategory::Render, "Metal: No index buffer bound for indexed draw (id={})", m_currentIndexBuffer.id);
        return;
    }

    // Check if vertex buffer is bound
    bool hasVertexBuffer = false;
    for (size_t i = 0; i < m_vertexBuffers.size(); ++i) {
        if (m_vertexBuffers[i].buffer.isValid()) {
            hasVertexBuffer = true;
            break;
        }
    }

    if (!hasVertexBuffer) {
        LOG_WARN(LogCategory::Render, "Metal: drawIndexed called but no vertex buffer bound!");
    }

    // Check if pipeline is bound
    auto pipeIt = m_state->pipelines.find(m_currentPipeline.id);
    if (pipeIt == m_state->pipelines.end()) {
        LOG_ERROR(LogCategory::Render, "Metal: drawIndexed called but pipeline {} not found!", m_currentPipeline.id);
        return;
    }

    LOG_INFO(LogCategory::Render, "Metal: drawIndexed indexCount={}, instanceCount={}, pipeline={}, hasVB={}, indexBuf={}",
             indexCount, instanceCount, m_currentPipeline.id, hasVertexBuffer, m_currentIndexBuffer.id);

    // Only bind material buffer if pushConstants wasn't used
    // pushConstants sets data directly via setVertexBytes/setFragmentBytes at index 16
    // Binding materialBuffer would overwrite that data
    if (!m_pushConstantsUsed) {
        updateMaterialBuffer();

        // Log uniform data for debugging (only when using material buffer path)
        float* uniformData = reinterpret_cast<float*>(m_materialData.data());
        LOG_DEBUG(LogCategory::Render, "Metal: u_ViewProjection[0-3]: {}, {}, {}, {}",
                  uniformData[0], uniformData[1], uniformData[2], uniformData[3]);
        LOG_DEBUG(LogCategory::Render, "Metal: u_Model[0-3]: {}, {}, {}, {}",
                  uniformData[16], uniformData[17], uniformData[18], uniformData[19]);
        LOG_DEBUG(LogCategory::Render, "Metal: u_TintColor: {}, {}, {}, {}",
                  uniformData[48], uniformData[49], uniformData[50], uniformData[51]);

        [m_encoder setVertexBuffer:m_materialBuffer offset:0 atIndex:16];
        [m_encoder setFragmentBuffer:m_materialBuffer offset:0 atIndex:16];
    } else {
        // Re-bind push constants if uniforms were modified after pushConstants was called
        // This is critical because applyPropertyOverrides sets uniforms (like u_UseTexture)
        // AFTER pushConstants, and we need to update the GPU with the new values
        if (m_pushConstantsDirty && m_pushConstantsSize > 0) {
            const int32_t* intData = reinterpret_cast<const int32_t*>(m_pushConstantsData.data());
            LOG_DEBUG(LogCategory::Render, "Metal: Re-binding pushConstants after uniform updates, u_UseTexture={}", intData[52]);

            [m_encoder setVertexBytes:m_pushConstantsData.data() length:m_pushConstantsSize atIndex:16];
            [m_encoder setFragmentBytes:m_pushConstantsData.data() length:m_pushConstantsSize atIndex:16];
            m_pushConstantsDirty = false;
        } else {
            LOG_DEBUG(LogCategory::Render, "Metal: Using pushConstants data (no updates needed)");
        }
    }

    // Always update and bind bone buffer (slot 17)
    updateBoneBuffer();
    [m_encoder setVertexBuffer:m_boneBuffer offset:0 atIndex:17];

    // Reset flag after draw so next draw uses correct path
    m_pushConstantsUsed = false;

    // Get primitive type from current pipeline
    MTLPrimitiveType primitiveType = pipeIt->second.mtlPrimitiveType;

    // Get index type and calculate offset
    MTLIndexType indexType = MetalUtils::toMTLIndexType(m_currentIndexFormat);
    uint32_t indexSize = (m_currentIndexFormat == IndexFormat::UInt16) ? 2 : 4;
    NSUInteger indexBufferOffset = m_indexBufferOffset + firstIndex * indexSize;

    // DEBUG: Log encoder and pipeline state
    LOG_DEBUG(LogCategory::Render, "Metal: drawIndexed encoder={}, pipelineState={}, primitiveType={}, indexType={}",
              (__bridge void*)m_encoder, (__bridge void*)pipeIt->second.pipelineState, (int)primitiveType, (int)indexType);

    [m_encoder drawIndexedPrimitives:primitiveType
                          indexCount:indexCount
                           indexType:indexType
                         indexBuffer:bufIt->second.buffer
                   indexBufferOffset:indexBufferOffset
                       instanceCount:instanceCount
                          baseVertex:vertexOffset
                        baseInstance:firstInstance];
}

void GfxCommandListMetal::barrier() {
    // In Metal, barriers are generally not needed between draw calls
    // Metal automatically handles synchronization within a render pass
    // For compute or cross-pass barriers, you would use MTLFence or MTLEvent

    // If we need to read from a texture we just wrote to, we'd need to end the encoder
    // and start a new one. For now, this is a no-op for render commands.
}

void GfxCommandListMetal::beginDebugMarker(const char* name) {
    ensureEncoder();
    if (!m_encoder) return;

    NSString* markerName = [NSString stringWithUTF8String:name];
    [m_encoder pushDebugGroup:markerName];
}

void GfxCommandListMetal::endDebugMarker() {
    if (!m_encoder) return;
    [m_encoder popDebugGroup];
}

// ============================================================================
// Compute Shader Support
// ============================================================================

void GfxCommandListMetal::ensureComputeEncoder() {
    if (m_computeEncoderActive) return;
    if (!m_commandBuffer) {
        // Create a new command buffer if needed
        m_commandBuffer = [m_state->commandQueue commandBuffer];
        if (!m_commandBuffer) {
            LOG_ERROR(LogCategory::Render, "Metal: Failed to create command buffer for compute");
            return;
        }
    }

    // End render encoder if active (can't have both at once)
    if (m_encoderActive) {
        endEncoder();
    }

    // Create compute command encoder
    m_computeEncoder = [m_commandBuffer computeCommandEncoder];
    if (!m_computeEncoder) {
        LOG_ERROR(LogCategory::Render, "Metal: Failed to create compute command encoder");
        return;
    }

    m_computeEncoderActive = true;
}

void GfxCommandListMetal::endComputeEncoder() {
    if (m_computeEncoderActive && m_computeEncoder) {
        [m_computeEncoder endEncoding];
        m_computeEncoder = nil;
        m_computeEncoderActive = false;
    }
}

void GfxCommandListMetal::bindComputePipeline(uint32_t pipelineId) {
    auto it = m_state->computePipelines.find(pipelineId);
    if (it == m_state->computePipelines.end()) {
        LOG_WARN(LogCategory::Render, "Metal: Invalid compute pipeline ID: {}", pipelineId);
        return;
    }

    ensureComputeEncoder();
    if (!m_computeEncoder) return;

    m_currentComputePipelineId = pipelineId;
    [m_computeEncoder setComputePipelineState:it->second.pipelineState];
}

void GfxCommandListMetal::bindComputeBuffer(BufferHandle buffer, uint32_t index) {
    auto it = m_state->buffers.find(buffer.id);
    if (it == m_state->buffers.end()) {
        LOG_WARN(LogCategory::Render, "Metal: Invalid compute buffer handle");
        return;
    }

    ensureComputeEncoder();
    if (!m_computeEncoder) return;

    [m_computeEncoder setBuffer:it->second.buffer offset:0 atIndex:index];
}

void GfxCommandListMetal::bindComputeTexture(TextureHandle texture, uint32_t index) {
    id<MTLTexture> mtlTexture = m_state->dummyTexture;

    auto it = m_state->textures.find(texture.id);
    if (it != m_state->textures.end()) {
        mtlTexture = it->second.texture;
    }

    ensureComputeEncoder();
    if (!m_computeEncoder) return;

    [m_computeEncoder setTexture:mtlTexture atIndex:index];
}

void GfxCommandListMetal::dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
    ensureComputeEncoder();
    if (!m_computeEncoder) return;

    auto it = m_state->computePipelines.find(m_currentComputePipelineId);
    if (it == m_state->computePipelines.end()) {
        LOG_WARN(LogCategory::Render, "Metal: No compute pipeline bound for dispatch");
        return;
    }

    const MetalComputePipeline& pipeline = it->second;

    // Dispatch thread groups
    MTLSize threadGroupCount = MTLSizeMake(groupCountX, groupCountY, groupCountZ);
    MTLSize threadGroupSize = pipeline.threadGroupSize;

    [m_computeEncoder dispatchThreadgroups:threadGroupCount
                     threadsPerThreadgroup:threadGroupSize];
}

void GfxCommandListMetal::dispatchIndirect(BufferHandle indirectBuffer, uint64_t offset) {
    auto bufIt = m_state->buffers.find(indirectBuffer.id);
    if (bufIt == m_state->buffers.end()) {
        LOG_WARN(LogCategory::Render, "Metal: Invalid indirect buffer for compute dispatch");
        return;
    }

    ensureComputeEncoder();
    if (!m_computeEncoder) return;

    auto pipeIt = m_state->computePipelines.find(m_currentComputePipelineId);
    if (pipeIt == m_state->computePipelines.end()) {
        LOG_WARN(LogCategory::Render, "Metal: No compute pipeline bound for indirect dispatch");
        return;
    }

    const MetalComputePipeline& pipeline = pipeIt->second;
    MTLSize threadGroupSize = pipeline.threadGroupSize;

    [m_computeEncoder dispatchThreadgroupsWithIndirectBuffer:bufIt->second.buffer
                                        indirectBufferOffset:offset
                                       threadsPerThreadgroup:threadGroupSize];
}

} // namespace lupine

#endif // __APPLE__
#endif // LUPINE_HAS_METAL
