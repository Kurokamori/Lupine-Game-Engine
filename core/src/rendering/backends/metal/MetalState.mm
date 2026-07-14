/**
 * Metal State Implementation
 *
 * This file contains the Metal backend state management and utility functions.
 * Uses Objective-C++ (.mm) for Metal API calls.
 */

#include "lupine/rendering/backends/metal/MetalState.hpp"
#include "lupine/logger/Logger.hpp"

#ifdef LUPINE_HAS_METAL
#ifdef __APPLE__

#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <QuartzCore/CAMetalLayer.h>
#import <TargetConditionals.h>

namespace lupine {

// ============================================================================
// MetalState Implementation
// ============================================================================

MetalState::MetalState() {
    // Initialize frame semaphore for triple buffering
    frameSemaphore = dispatch_semaphore_create(MAX_FRAMES_IN_FLIGHT);
}

MetalState::~MetalState() {
    flushDeferredDestructions();
    destroyDummyResources();

    // Release frame semaphore
    if (frameSemaphore) {
        // Wait for all frames to complete
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            dispatch_semaphore_wait(frameSemaphore, DISPATCH_TIME_FOREVER);
        }
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            dispatch_semaphore_signal(frameSemaphore);
        }
    }

    // Release Metal objects (ARC will handle this, but we nil them for safety)
    currentCommandBuffer = nil;
    currentEncoder = nil;
    commandQueue = nil;
    device = nil;
}

void MetalState::createDummyResources() {
    if (!device) return;

    // Create a 1x1 white texture
    MTLTextureDescriptor* texDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                                        width:1
                                                                                       height:1
                                                                                    mipmapped:NO];
    texDesc.usage = MTLTextureUsageShaderRead;
    dummyTexture = [device newTextureWithDescriptor:texDesc];

    if (dummyTexture) {
        uint8_t whitePixel[4] = {255, 255, 255, 255};
        MTLRegion region = MTLRegionMake2D(0, 0, 1, 1);
        [dummyTexture replaceRegion:region mipmapLevel:0 withBytes:whitePixel bytesPerRow:4];
    }

    // Create dummy sampler
    MTLSamplerDescriptor* samplerDesc = [[MTLSamplerDescriptor alloc] init];
    samplerDesc.minFilter = MTLSamplerMinMagFilterLinear;
    samplerDesc.magFilter = MTLSamplerMinMagFilterLinear;
    samplerDesc.mipFilter = MTLSamplerMipFilterLinear;
    samplerDesc.sAddressMode = MTLSamplerAddressModeRepeat;
    samplerDesc.tAddressMode = MTLSamplerAddressModeRepeat;
    samplerDesc.rAddressMode = MTLSamplerAddressModeRepeat;
    dummySampler = [device newSamplerStateWithDescriptor:samplerDesc];

    // Create dummy buffer (256 bytes, zeros)
    dummyBuffer = [device newBufferWithLength:256 options:MTLResourceStorageModeShared];

    LOG_DEBUG(LogCategory::Render, "Metal: Created dummy resources");
}

void MetalState::destroyDummyResources() {
    dummyTexture = nil;
    dummySampler = nil;
    dummyBuffer = nil;
}

void MetalState::deferPipelineDestroy(const MetalPipeline& pipeline) {
    deferredPipelineDestroys.push_back({pipeline, DEFERRED_DESTROY_FRAMES});
}

void MetalState::deferComputePipelineDestroy(const MetalComputePipeline& pipeline) {
    deferredComputePipelineDestroys.push_back({pipeline, DEFERRED_DESTROY_FRAMES});
}

void MetalState::deferShaderDestroy(const MetalShader& shader) {
    deferredShaderDestroys.push_back({shader, DEFERRED_DESTROY_FRAMES});
}

void MetalState::deferBufferDestroy(const MetalBuffer& buffer) {
    deferredBufferDestroys.push_back({buffer, DEFERRED_DESTROY_FRAMES});
}

void MetalState::deferTextureDestroy(const MetalTexture& texture) {
    deferredTextureDestroys.push_back({texture, DEFERRED_DESTROY_FRAMES});
}

void MetalState::processDeferredDestructions() {
    // Process render pipelines
    for (auto it = deferredPipelineDestroys.begin(); it != deferredPipelineDestroys.end();) {
        if (--it->framesRemaining == 0) {
            // Pipeline will be released by ARC when struct goes out of scope
            it = deferredPipelineDestroys.erase(it);
        } else {
            ++it;
        }
    }

    // Process compute pipelines
    for (auto it = deferredComputePipelineDestroys.begin(); it != deferredComputePipelineDestroys.end();) {
        if (--it->framesRemaining == 0) {
            it = deferredComputePipelineDestroys.erase(it);
        } else {
            ++it;
        }
    }

    // Process shaders
    for (auto it = deferredShaderDestroys.begin(); it != deferredShaderDestroys.end();) {
        if (--it->framesRemaining == 0) {
            it = deferredShaderDestroys.erase(it);
        } else {
            ++it;
        }
    }

    // Process buffers
    for (auto it = deferredBufferDestroys.begin(); it != deferredBufferDestroys.end();) {
        if (--it->framesRemaining == 0) {
            it = deferredBufferDestroys.erase(it);
        } else {
            ++it;
        }
    }

    // Process textures
    for (auto it = deferredTextureDestroys.begin(); it != deferredTextureDestroys.end();) {
        if (--it->framesRemaining == 0) {
            it = deferredTextureDestroys.erase(it);
        } else {
            ++it;
        }
    }
}

void MetalState::flushDeferredDestructions() {
    deferredPipelineDestroys.clear();
    deferredComputePipelineDestroys.clear();
    deferredShaderDestroys.clear();
    deferredBufferDestroys.clear();
    deferredTextureDestroys.clear();
}

// ============================================================================
// MetalUtils Implementation
// ============================================================================

namespace MetalUtils {

MTLPixelFormat toMTLPixelFormat(TextureFormat format) {
    switch (format) {
        case TextureFormat::R8_UNORM:           return MTLPixelFormatR8Unorm;
        case TextureFormat::RG8_UNORM:          return MTLPixelFormatRG8Unorm;
        case TextureFormat::RGBA8_UNORM:        return MTLPixelFormatRGBA8Unorm;
        case TextureFormat::RGBA8_SRGB:         return MTLPixelFormatRGBA8Unorm_sRGB;
        case TextureFormat::BGRA8_UNORM:        return MTLPixelFormatBGRA8Unorm;
        case TextureFormat::BGRA8_SRGB:         return MTLPixelFormatBGRA8Unorm_sRGB;

        case TextureFormat::R16_FLOAT:          return MTLPixelFormatR16Float;
        case TextureFormat::RG16_FLOAT:         return MTLPixelFormatRG16Float;
        case TextureFormat::RGBA16_FLOAT:       return MTLPixelFormatRGBA16Float;

        case TextureFormat::R32_FLOAT:          return MTLPixelFormatR32Float;
        case TextureFormat::RG32_FLOAT:         return MTLPixelFormatRG32Float;
        case TextureFormat::RGB32_FLOAT:        return MTLPixelFormatRGBA32Float; // Metal doesn't have RGB32, use RGBA32
        case TextureFormat::RGBA32_FLOAT:       return MTLPixelFormatRGBA32Float;

        case TextureFormat::DEPTH16:            return MTLPixelFormatDepth16Unorm;
        case TextureFormat::DEPTH24:            return MTLPixelFormatDepth32Float; // Metal doesn't have Depth24, use Depth32
        case TextureFormat::DEPTH32F:           return MTLPixelFormatDepth32Float;
        case TextureFormat::DEPTH24_STENCIL8:   return MTLPixelFormatDepth32Float_Stencil8; // Metal uses Depth32+Stencil8
        case TextureFormat::DEPTH32F_STENCIL8:  return MTLPixelFormatDepth32Float_Stencil8;

        // Compressed formats (BC/DXT)
        case TextureFormat::BC1_RGB:            return MTLPixelFormatBC1_RGBA;
        case TextureFormat::BC1_RGBA:           return MTLPixelFormatBC1_RGBA;
        case TextureFormat::BC3_RGBA:           return MTLPixelFormatBC3_RGBA;
        case TextureFormat::BC5_RG:             return MTLPixelFormatBC5_RGUnorm;
        case TextureFormat::BC7_RGBA:           return MTLPixelFormatBC7_RGBAUnorm;

        default:
            LOG_WARN(LogCategory::Render, "Metal: Unknown texture format, defaulting to RGBA8");
            return MTLPixelFormatRGBA8Unorm;
    }
}

MTLPixelFormat toMTLPixelFormatDepth(TextureFormat format) {
    switch (format) {
        case TextureFormat::DEPTH16:            return MTLPixelFormatDepth16Unorm;
        case TextureFormat::DEPTH24:            return MTLPixelFormatDepth32Float;
        case TextureFormat::DEPTH32F:           return MTLPixelFormatDepth32Float;
        case TextureFormat::DEPTH24_STENCIL8:   return MTLPixelFormatDepth32Float_Stencil8;
        case TextureFormat::DEPTH32F_STENCIL8:  return MTLPixelFormatDepth32Float_Stencil8;
        default:
            return MTLPixelFormatDepth32Float;
    }
}

MTLPrimitiveType toMTLPrimitiveType(PrimitiveTopology topology) {
    switch (topology) {
        case PrimitiveTopology::PointList:      return MTLPrimitiveTypePoint;
        case PrimitiveTopology::LineList:       return MTLPrimitiveTypeLine;
        case PrimitiveTopology::LineStrip:      return MTLPrimitiveTypeLineStrip;
        case PrimitiveTopology::TriangleList:   return MTLPrimitiveTypeTriangle;
        case PrimitiveTopology::TriangleStrip:  return MTLPrimitiveTypeTriangleStrip;
        default:                                return MTLPrimitiveTypeTriangle;
    }
}

MTLCompareFunction toMTLCompareFunction(CompareFunc func) {
    switch (func) {
        case CompareFunc::Never:        return MTLCompareFunctionNever;
        case CompareFunc::Less:         return MTLCompareFunctionLess;
        case CompareFunc::Equal:        return MTLCompareFunctionEqual;
        case CompareFunc::LessEqual:    return MTLCompareFunctionLessEqual;
        case CompareFunc::Greater:      return MTLCompareFunctionGreater;
        case CompareFunc::NotEqual:     return MTLCompareFunctionNotEqual;
        case CompareFunc::GreaterEqual: return MTLCompareFunctionGreaterEqual;
        case CompareFunc::Always:       return MTLCompareFunctionAlways;
        default:                        return MTLCompareFunctionLess;
    }
}

MTLBlendFactor toMTLBlendFactor(BlendFactor factor) {
    switch (factor) {
        case BlendFactor::Zero:                 return MTLBlendFactorZero;
        case BlendFactor::One:                  return MTLBlendFactorOne;
        case BlendFactor::SrcColor:             return MTLBlendFactorSourceColor;
        case BlendFactor::OneMinusSrcColor:     return MTLBlendFactorOneMinusSourceColor;
        case BlendFactor::DstColor:             return MTLBlendFactorDestinationColor;
        case BlendFactor::OneMinusDstColor:     return MTLBlendFactorOneMinusDestinationColor;
        case BlendFactor::SrcAlpha:             return MTLBlendFactorSourceAlpha;
        case BlendFactor::OneMinusSrcAlpha:     return MTLBlendFactorOneMinusSourceAlpha;
        case BlendFactor::DstAlpha:             return MTLBlendFactorDestinationAlpha;
        case BlendFactor::OneMinusDstAlpha:     return MTLBlendFactorOneMinusDestinationAlpha;
        case BlendFactor::ConstantColor:        return MTLBlendFactorBlendColor;
        case BlendFactor::OneMinusConstantColor: return MTLBlendFactorOneMinusBlendColor;
        default:                                return MTLBlendFactorOne;
    }
}

MTLBlendOperation toMTLBlendOperation(BlendOp op) {
    switch (op) {
        case BlendOp::Add:              return MTLBlendOperationAdd;
        case BlendOp::Subtract:         return MTLBlendOperationSubtract;
        case BlendOp::ReverseSubtract:  return MTLBlendOperationReverseSubtract;
        case BlendOp::Min:              return MTLBlendOperationMin;
        case BlendOp::Max:              return MTLBlendOperationMax;
        default:                        return MTLBlendOperationAdd;
    }
}

MTLSamplerMinMagFilter toMTLFilter(FilterMode mode) {
    switch (mode) {
        case FilterMode::Nearest:   return MTLSamplerMinMagFilterNearest;
        case FilterMode::Linear:    return MTLSamplerMinMagFilterLinear;
        default:                    return MTLSamplerMinMagFilterLinear;
    }
}

MTLSamplerMipFilter toMTLMipFilter(FilterMode mode) {
    switch (mode) {
        case FilterMode::Nearest:   return MTLSamplerMipFilterNearest;
        case FilterMode::Linear:    return MTLSamplerMipFilterLinear;
        default:                    return MTLSamplerMipFilterLinear;
    }
}

MTLSamplerAddressMode toMTLAddressMode(WrapMode mode) {
    switch (mode) {
        case WrapMode::Repeat:          return MTLSamplerAddressModeRepeat;
        case WrapMode::MirroredRepeat:  return MTLSamplerAddressModeMirrorRepeat;
        case WrapMode::ClampToEdge:     return MTLSamplerAddressModeClampToEdge;
        case WrapMode::ClampToBorder:   return MTLSamplerAddressModeClampToBorderColor;
        default:                        return MTLSamplerAddressModeRepeat;
    }
}

MTLCullMode toMTLCullMode(CullMode mode) {
    switch (mode) {
        case CullMode::None:    return MTLCullModeNone;
        case CullMode::Front:   return MTLCullModeFront;
        case CullMode::Back:    return MTLCullModeBack;
        default:                return MTLCullModeBack;
    }
}

MTLWinding toMTLWinding(WindingOrder order) {
    switch (order) {
        case WindingOrder::Clockwise:           return MTLWindingClockwise;
        case WindingOrder::CounterClockwise:    return MTLWindingCounterClockwise;
        default:                                return MTLWindingCounterClockwise;
    }
}

MTLTriangleFillMode toMTLFillMode(FillMode mode) {
    switch (mode) {
        case FillMode::Solid:       return MTLTriangleFillModeFill;
        case FillMode::Wireframe:   return MTLTriangleFillModeLines;
        default:                    return MTLTriangleFillModeFill;
    }
}

MTLVertexFormat toMTLVertexFormat(VertexFormat format) {
    switch (format) {
        case VertexFormat::Float:   return MTLVertexFormatFloat;
        case VertexFormat::Float2:  return MTLVertexFormatFloat2;
        case VertexFormat::Float3:  return MTLVertexFormatFloat3;
        case VertexFormat::Float4:  return MTLVertexFormatFloat4;
        case VertexFormat::Int:     return MTLVertexFormatInt;
        case VertexFormat::Int2:    return MTLVertexFormatInt2;
        case VertexFormat::Int3:    return MTLVertexFormatInt3;
        case VertexFormat::Int4:    return MTLVertexFormatInt4;
        case VertexFormat::UInt:    return MTLVertexFormatUInt;
        case VertexFormat::UInt2:   return MTLVertexFormatUInt2;
        case VertexFormat::UInt3:   return MTLVertexFormatUInt3;
        case VertexFormat::UInt4:   return MTLVertexFormatUInt4;
        default:                    return MTLVertexFormatFloat3;
    }
}

MTLIndexType toMTLIndexType(IndexFormat format) {
    switch (format) {
        case IndexFormat::UInt16:   return MTLIndexTypeUInt16;
        case IndexFormat::UInt32:   return MTLIndexTypeUInt32;
        default:                    return MTLIndexTypeUInt32;
    }
}

bool hasStencilComponent(MTLPixelFormat format) {
    switch (format) {
        case MTLPixelFormatDepth32Float_Stencil8:
        case MTLPixelFormatStencil8:
        case MTLPixelFormatX32_Stencil8:
            return true;
        default:
            return false;
    }
}

bool isDepthFormat(TextureFormat format) {
    switch (format) {
        case TextureFormat::DEPTH16:
        case TextureFormat::DEPTH24:
        case TextureFormat::DEPTH32F:
        case TextureFormat::DEPTH24_STENCIL8:
        case TextureFormat::DEPTH32F_STENCIL8:
            return true;
        default:
            return false;
    }
}

uint32_t getBytesPerPixel(MTLPixelFormat format) {
    switch (format) {
        case MTLPixelFormatR8Unorm:
        case MTLPixelFormatR8Snorm:
        case MTLPixelFormatR8Uint:
        case MTLPixelFormatR8Sint:
            return 1;

        case MTLPixelFormatR16Float:
        case MTLPixelFormatR16Unorm:
        case MTLPixelFormatR16Snorm:
        case MTLPixelFormatR16Uint:
        case MTLPixelFormatR16Sint:
        case MTLPixelFormatRG8Unorm:
        case MTLPixelFormatRG8Snorm:
        case MTLPixelFormatRG8Uint:
        case MTLPixelFormatRG8Sint:
        case MTLPixelFormatDepth16Unorm:
            return 2;

        case MTLPixelFormatDepth32Float:
        case MTLPixelFormatR32Float:
        case MTLPixelFormatR32Uint:
        case MTLPixelFormatR32Sint:
        case MTLPixelFormatRG16Float:
        case MTLPixelFormatRG16Unorm:
        case MTLPixelFormatRG16Snorm:
        case MTLPixelFormatRG16Uint:
        case MTLPixelFormatRG16Sint:
        case MTLPixelFormatRGBA8Unorm:
        case MTLPixelFormatRGBA8Unorm_sRGB:
        case MTLPixelFormatRGBA8Snorm:
        case MTLPixelFormatRGBA8Uint:
        case MTLPixelFormatRGBA8Sint:
        case MTLPixelFormatBGRA8Unorm:
        case MTLPixelFormatBGRA8Unorm_sRGB:
            return 4;

        case MTLPixelFormatDepth32Float_Stencil8:
            return 5; // Actually 8 bytes due to alignment, but 5 bytes of data

        case MTLPixelFormatRG32Float:
        case MTLPixelFormatRG32Uint:
        case MTLPixelFormatRG32Sint:
        case MTLPixelFormatRGBA16Float:
        case MTLPixelFormatRGBA16Unorm:
        case MTLPixelFormatRGBA16Snorm:
        case MTLPixelFormatRGBA16Uint:
        case MTLPixelFormatRGBA16Sint:
            return 8;

        case MTLPixelFormatRGBA32Float:
        case MTLPixelFormatRGBA32Uint:
        case MTLPixelFormatRGBA32Sint:
            return 16;

        default:
            return 4; // Default assumption
    }
}

uint32_t getVertexFormatSize(VertexFormat format) {
    switch (format) {
        case VertexFormat::Float:   return 4;
        case VertexFormat::Float2:  return 8;
        case VertexFormat::Float3:  return 12;
        case VertexFormat::Float4:  return 16;
        case VertexFormat::Int:     return 4;
        case VertexFormat::Int2:    return 8;
        case VertexFormat::Int3:    return 12;
        case VertexFormat::Int4:    return 16;
        case VertexFormat::UInt:    return 4;
        case VertexFormat::UInt2:   return 8;
        case VertexFormat::UInt3:   return 12;
        case VertexFormat::UInt4:   return 16;
        default:                    return 12;
    }
}

MTLRenderPassDescriptor* createRenderPassDescriptor(
    id<MTLTexture> colorTexture,
    id<MTLTexture> depthTexture,
    MTLLoadAction colorLoadAction,
    MTLLoadAction depthLoadAction,
    MTLClearColor clearColor,
    double clearDepth
) {
    MTLRenderPassDescriptor* descriptor = [MTLRenderPassDescriptor renderPassDescriptor];

    if (colorTexture) {
        descriptor.colorAttachments[0].texture = colorTexture;
        descriptor.colorAttachments[0].loadAction = colorLoadAction;
        descriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
        descriptor.colorAttachments[0].clearColor = clearColor;
    }

    if (depthTexture) {
        descriptor.depthAttachment.texture = depthTexture;
        descriptor.depthAttachment.loadAction = depthLoadAction;
        descriptor.depthAttachment.storeAction = MTLStoreActionStore;
        descriptor.depthAttachment.clearDepth = clearDepth;

        // Check if the depth texture also has stencil
        if (hasStencilComponent(depthTexture.pixelFormat)) {
            descriptor.stencilAttachment.texture = depthTexture;
            descriptor.stencilAttachment.loadAction = depthLoadAction;
            descriptor.stencilAttachment.storeAction = MTLStoreActionStore;
            descriptor.stencilAttachment.clearStencil = 0;
        }
    }

    return descriptor;
}

void extractShaderReflection(MetalShader& shader) {
    if (!shader.function) return;

    // Clear existing reflection data
    shader.uniforms.clear();
    shader.textureBindings.clear();
    shader.bufferBindings.clear();

    // Get function vertex/fragment attributes based on stage
    // Note: Full reflection requires creating a pipeline, so we extract basic info here

    // For Metal, we use function constants and argument inspection
    // The detailed buffer layout requires pipeline reflection (done in extractPipelineReflection)

    // Extract texture bindings from function if available
    // Note: Metal's function reflection API is limited; full reflection happens at pipeline creation
    LOG_DEBUG(LogCategory::Render, "Metal: Shader reflection extracted for function '{}'",
              [shader.function.name UTF8String]);
}

// ============================================================================
// iOS/TBDR Optimizations
// ============================================================================

MTLStorageMode getOptimalStorageMode(id<MTLDevice> device, TextureUsage usage, bool isTransient) {
#if TARGET_OS_IOS || TARGET_OS_TV
    // iOS/tvOS: Use memoryless storage for transient render targets
    // This is a major bandwidth optimization on tile-based GPUs
    if (isTransient && ((usage & TextureUsage::RenderTarget) != TextureUsage::None ||
                        (usage & TextureUsage::DepthStencil) != TextureUsage::None)) {
        return MTLStorageModeMemoryless;
    }
    // iOS has unified memory, so shared mode is efficient
    if ((usage & TextureUsage::Sampled) != TextureUsage::None) {
        return MTLStorageModeShared;
    }
    return MTLStorageModePrivate;
#else
    // macOS: Use private storage for GPU-only textures, shared for CPU-accessible
    if (device.hasUnifiedMemory) {
        // Apple Silicon Mac - similar to iOS
        if (isTransient && ((usage & TextureUsage::RenderTarget) != TextureUsage::None ||
                            (usage & TextureUsage::DepthStencil) != TextureUsage::None)) {
            // Note: MTLStorageModeMemoryless is not available on macOS
            return MTLStorageModePrivate;
        }
        return MTLStorageModeShared;
    } else {
        // Intel/AMD Mac - use private for GPU-only
        return MTLStorageModePrivate;
    }
#endif
}

MTLResourceOptions getOptimalBufferOptions(id<MTLDevice> device, bool cpuWritable) {
#if TARGET_OS_IOS || TARGET_OS_TV
    // iOS: Always use shared storage for buffers (unified memory)
    return MTLResourceStorageModeShared;
#else
    if (device.hasUnifiedMemory) {
        // Apple Silicon Mac
        return MTLResourceStorageModeShared;
    } else {
        // Intel/AMD Mac
        if (cpuWritable) {
            return MTLResourceStorageModeShared;
        } else {
            return MTLResourceStorageModePrivate;
        }
    }
#endif
}

bool supportsMemorylessStorage(id<MTLDevice> device) {
#if TARGET_OS_IOS || TARGET_OS_TV
    // Memoryless storage is supported on iOS/tvOS A8 and later
    return true;
#else
    // Memoryless is not available on macOS
    (void)device;
    return false;
#endif
}

bool hasUnifiedMemory(id<MTLDevice> device) {
    return device.hasUnifiedMemory;
}

void extractPipelineReflection(MetalPipeline& pipeline, MTLRenderPipelineReflection* reflection) {
    if (!reflection) return;

    // Clear existing uniform data
    pipeline.uniformOffsets.clear();
    pipeline.uniformBufferSize = 0;

    // Process vertex arguments
    for (MTLArgument* arg in reflection.vertexArguments) {
        if (arg.type == MTLArgumentTypeBuffer && arg.bufferDataType != MTLDataTypeNone) {
            // This is a buffer argument - extract struct members for uniforms
            MTLStructType* structType = arg.bufferStructType;
            if (structType) {
                for (MTLStructMember* member in structType.members) {
                    NSString* memberName = member.name;
                    std::string name = [memberName UTF8String];

                    // Calculate size based on data type
                    uint32_t size = 0;
                    switch (member.dataType) {
                        case MTLDataTypeFloat:  size = 4; break;
                        case MTLDataTypeFloat2: size = 8; break;
                        case MTLDataTypeFloat3: size = 12; break;
                        case MTLDataTypeFloat4: size = 16; break;
                        case MTLDataTypeInt:    size = 4; break;
                        case MTLDataTypeInt2:   size = 8; break;
                        case MTLDataTypeInt3:   size = 12; break;
                        case MTLDataTypeInt4:   size = 16; break;
                        case MTLDataTypeFloat4x4: size = 64; break;
                        case MTLDataTypeFloat3x3: size = 48; break;
                        default: size = 16; break; // Default assumption
                    }

                    pipeline.uniformOffsets[name] = static_cast<uint32_t>(member.offset);
                    pipeline.uniformBufferSize = std::max(pipeline.uniformBufferSize,
                        static_cast<uint32_t>(member.offset + size));

                    LOG_DEBUG(LogCategory::Render, "Metal: Reflected vertex uniform '{}' at offset {} size {}",
                              name, member.offset, size);
                }
            }
        } else if (arg.type == MTLArgumentTypeTexture) {
            LOG_DEBUG(LogCategory::Render, "Metal: Reflected vertex texture '{}' at index {}",
                      [arg.name UTF8String], arg.index);
        } else if (arg.type == MTLArgumentTypeSampler) {
            LOG_DEBUG(LogCategory::Render, "Metal: Reflected vertex sampler '{}' at index {}",
                      [arg.name UTF8String], arg.index);
        }
    }

    // Process fragment arguments
    for (MTLArgument* arg in reflection.fragmentArguments) {
        if (arg.type == MTLArgumentTypeBuffer && arg.bufferDataType != MTLDataTypeNone) {
            MTLStructType* structType = arg.bufferStructType;
            if (structType) {
                for (MTLStructMember* member in structType.members) {
                    NSString* memberName = member.name;
                    std::string name = [memberName UTF8String];

                    // Skip if already defined in vertex shader
                    if (pipeline.uniformOffsets.find(name) != pipeline.uniformOffsets.end()) {
                        continue;
                    }

                    uint32_t size = 0;
                    switch (member.dataType) {
                        case MTLDataTypeFloat:  size = 4; break;
                        case MTLDataTypeFloat2: size = 8; break;
                        case MTLDataTypeFloat3: size = 12; break;
                        case MTLDataTypeFloat4: size = 16; break;
                        case MTLDataTypeInt:    size = 4; break;
                        case MTLDataTypeInt2:   size = 8; break;
                        case MTLDataTypeInt3:   size = 12; break;
                        case MTLDataTypeInt4:   size = 16; break;
                        case MTLDataTypeFloat4x4: size = 64; break;
                        case MTLDataTypeFloat3x3: size = 48; break;
                        default: size = 16; break;
                    }

                    pipeline.uniformOffsets[name] = static_cast<uint32_t>(member.offset);
                    pipeline.uniformBufferSize = std::max(pipeline.uniformBufferSize,
                        static_cast<uint32_t>(member.offset + size));

                    LOG_DEBUG(LogCategory::Render, "Metal: Reflected fragment uniform '{}' at offset {} size {}",
                              name, member.offset, size);
                }
            }
        } else if (arg.type == MTLArgumentTypeTexture) {
            LOG_DEBUG(LogCategory::Render, "Metal: Reflected fragment texture '{}' at index {}",
                      [arg.name UTF8String], arg.index);
        } else if (arg.type == MTLArgumentTypeSampler) {
            LOG_DEBUG(LogCategory::Render, "Metal: Reflected fragment sampler '{}' at index {}",
                      [arg.name UTF8String], arg.index);
        }
    }

    // Align total size to 16 bytes
    pipeline.uniformBufferSize = (pipeline.uniformBufferSize + 15) & ~15;

    LOG_DEBUG(LogCategory::Render, "Metal: Pipeline reflection complete - {} uniforms, total size {} bytes",
              pipeline.uniformOffsets.size(), pipeline.uniformBufferSize);
}

} // namespace MetalUtils

} // namespace lupine

#endif // __APPLE__
#endif // LUPINE_HAS_METAL
